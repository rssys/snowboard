/*
 *  i386 emulator main execution loop
 *
 *  Copyright (c) 2003-2005 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include "cpu.h"
#include "disas.h"
#include "tcg.h"
#include "qemu-barrier.h"

#include "sysemu.h"
#include "ski.h"
#include "ski-liveness.h"

#include "ski-heuristics.h"
#include "ski-heuristics-statistics.h"

int tb_invalidated_flag;
extern int ski_forkall_enabled;
extern int ski_init_options_trace_instructions_enabled;
//#define CONFIG_DEBUG_EXEC

bool qemu_cpu_has_work(CPUState *env)
{
    return cpu_has_work(env);
}

void cpu_loop_exit(CPUState *env)
{
    env->current_tb = NULL;
    longjmp(env->jmp_env, 1);
}

/* exit the current TB from a signal handler. The host registers are
   restored in a state compatible with the CPU emulator
 */
#if defined(CONFIG_SOFTMMU)
void cpu_resume_from_signal(CPUState *env, void *puc)
{
    /* XXX: restore cpu registers saved in host registers */

    env->exception_index = -1;
    longjmp(env->jmp_env, 1);
}
#endif

/* Execute the code without caching the generated code. An interpreter
   could be used if available. */
static void cpu_exec_nocache(CPUState *env, int max_cycles,
                             TranslationBlock *orig_tb)
{
    unsigned long next_tb;
    TranslationBlock *tb;

    /* Should never happen.
       We only end up here when an existing TB is too long.  */
    if (max_cycles > CF_COUNT_MASK)
        max_cycles = CF_COUNT_MASK;

// PF
//	int d;
//	printf("Piking: Test1 (CPUState: %p)\n", env);
//	sscanf("%d\n", &d);

    tb = tb_gen_code(env, orig_tb->pc, orig_tb->cs_base, orig_tb->flags,
                     max_cycles);
    env->current_tb = tb;
    /* execute the generated code */
    next_tb = tcg_qemu_tb_exec(env, tb->tc_ptr);
    env->current_tb = NULL;

    if ((next_tb & 3) == 2) {
        /* Restore PC.  This may happen if async event occurs before
           the TB starts executing.  */
        cpu_pc_from_tb(env, tb);
    }
    tb_phys_invalidate(tb, -1);
    tb_free(tb);
}

static TranslationBlock *tb_find_slow(CPUState *env,
                                      target_ulong pc,
                                      target_ulong cs_base,
                                      uint64_t flags)
{
    TranslationBlock *tb, **ptb1;
    unsigned int h;
    tb_page_addr_t phys_pc, phys_page1;
    target_ulong virt_page2;

    tb_invalidated_flag = 0;

    /* find translated block using physical mappings */
    phys_pc = get_page_addr_code(env, pc);
    phys_page1 = phys_pc & TARGET_PAGE_MASK;
    h = tb_phys_hash_func(phys_pc);
    ptb1 = &tb_phys_hash[h];
    for(;;) {
        tb = *ptb1;
        if (!tb)
            goto not_found;
        if (tb->pc == pc &&
            tb->page_addr[0] == phys_page1 &&
            tb->cs_base == cs_base &&
            tb->flags == flags) {
            /* check next page if needed */
            if (tb->page_addr[1] != -1) {
                tb_page_addr_t phys_page2;

                virt_page2 = (pc & TARGET_PAGE_MASK) +
                    TARGET_PAGE_SIZE;
                phys_page2 = get_page_addr_code(env, virt_page2);
                if (tb->page_addr[1] == phys_page2)
                    goto found;
            } else {
                goto found;
            }
        }
        ptb1 = &tb->phys_hash_next;
    }
 not_found:
   /* if no translated code available, then translate it now */
    tb = tb_gen_code(env, pc, cs_base, flags, 0);

 found:
    /* Move the last found TB to the head of the list */
    if (likely(*ptb1)) {
        *ptb1 = tb->phys_hash_next;
        tb->phys_hash_next = tb_phys_hash[h];
        tb_phys_hash[h] = tb;
    }
    /* we add the TB in the virtual pc hash table */
    env->tb_jmp_cache[tb_jmp_cache_hash_func(pc)] = tb;
    return tb;
}

static inline TranslationBlock *tb_find_fast(CPUState *env)
{
    TranslationBlock *tb;
    target_ulong cs_base, pc;
    int flags;

    /* we record a subset of the CPU state. It will
       always be the same before a given translated block
       is executed. */
    cpu_get_tb_cpu_state(env, &pc, &cs_base, &flags);
    tb = env->tb_jmp_cache[tb_jmp_cache_hash_func(pc)];
    if (unlikely(!tb || tb->pc != pc || tb->cs_base != cs_base ||
                 tb->flags != flags)) {
        tb = tb_find_slow(env, pc, cs_base, flags);
    }
    return tb;
}

static CPUDebugExcpHandler *debug_excp_handler;

CPUDebugExcpHandler *cpu_set_debug_excp_handler(CPUDebugExcpHandler *handler)
{
    CPUDebugExcpHandler *old_handler = debug_excp_handler;

    debug_excp_handler = handler;
    return old_handler;
}

static void cpu_handle_debug_exception(CPUState *env)
{
    CPUWatchpoint *wp;

    if (!env->watchpoint_hit) {
        QTAILQ_FOREACH(wp, &env->watchpoints, entry) {
            wp->flags &= ~BP_WATCHPOINT_HIT;
        }
    }
    if (debug_excp_handler) {
        debug_excp_handler(env);
    }
}

// *************************************************************************************************
//                                 SKI
// *************************************************************************************************

static void ski_tc_initialize(thread_context *tc, int id, int priority, int cpu_no, int int_no, int reentry_no);

//void ski_threads_dump(CPUState *env);
static void ski_threads_initialize(CPUState *env);
static thread_context* ski_threads_find_next_runnable(CPUState *env);

static thread_context* ski_threads_find(CPUState *env, int exists, int cpu_no, int is_int, int int_no, int reentry_no, int state_mask);
static int ski_cpu_can_start_interrupts(CPUState* env);
static int ski_interrupts_is_top_hw_stack(CPUState*env, thread_context *tc);
int ski_cpus_dump(CPUState *env);
void ski_interrupts_dump_stack_all(CPUState* env);

thread_context ski_threads[MAX_SKI_THREADS];
int ski_threads_no = 0;
thread_context* ski_threads_current = 0;
int ski_threads_need_reevaluate = 0;

int ski_sched_instructions_current = 0;
int ski_sched_instructions_total = 0;

int ski_sched_cpu1_do_preemption = 0;
int ski_sched_cpu2_do_preemption = 0;
extern int ski_sched_cpu1_preemption_point;
extern int ski_sched_cpu2_preemption_point;
extern int ski_sched_cpu1_access;
extern int ski_sched_cpu2_access;
int ski_apic_disable_timer = 0;
int ski_apic_disable_all = 0;
int ski_block_other_cpus = 1;
int ski_snapshot_restoring = 0;

int ski_global_init_preemmption=-1;

extern char ski_init_options_priorities_filename[];
extern int ski_init_options_input_number[SKI_CPUS_MAX];

extern int ski_init_options_cpu_1_mode;
extern int ski_init_options_cpu_2_mode;

// -------------------------------------------------------------------------------------------------
// These two functions (ski_snapshot_reinitialize and ski_snapshot_reinitialize_cpu) contain 
//   the code necessary to reinitialize our internal data structures when resuming from a snapshot.
// Variable values that are stored in the snapshot should not be initialized here. 
void ski_snapshot_reinitialize(CPUState* env){
	ski_trace_active=true;
    SKI_TRACE("ski_snapshot_reinitialize()\n");

	//From: ski_first_in_test(env);
    ski_sched_instructions_current = 0;
    ski_sched_instructions_total = 0;

	ski_exec_instruction_counter_total = 0;
	ski_exec_instruction_counter_per_cycle = 0;  // This one is reset when we loop in cpu-exec.c (not very usefull)

    ski_exec_trace_start(env);
    ski_apic_disable_timer = 1;
    ski_apic_disable_all = 1;
    //ski_disable_clock();

    if(ski_init_options_preemptions_len){
		// find the lowest value in SKI preeptions lists
        ski_global_init_preemmption = ski_preemption_get_and_clear_lowest(env, -1);
    }

}

int ski_preemption_get_and_clear_lowest(CPUState*env, int min_required){
	int i;
	int *min_found = 0;
	int ret = 0;
	for(i=0;i<ski_init_options_preemptions_len;i++){
		int *p = &ski_init_options_preemptions[i];
		SKI_TRACE("ski_preemption_get_and_clear_lowest: i = %d, min_found = %d, p = %d\n", i, min_found , p);
		if((min_found == 0) && (*p >= min_required)){
			// Initialize
			SKI_TRACE("ski_preemption_get_and_clear_lowest: 1\n");
			min_found = p;
		}else if ( min_found && (*min_found > *p) && (*p >= min_required)){
			SKI_TRACE("ski_preemption_get_and_clear_lowest: 2\n");
			min_found = p;
		}
	}
	if(min_found){
		ret = *min_found;
		// Clear the value
		*min_found = -1;
	}else{
		// If not found, return -1 
		ret = -1;
	}
	SKI_TRACE("ski_preemption_get_and_clear_lowest: returning value = %d (min_required = %d)\n", ret, min_required);
	return ret;
}

void ski_snapshot_reinitialize_cpu(CPUState* env){
	hypercall_io hio;
	// Code from the enter hypercall in ski-hyper.c

	// Restored from the snapshot:
	//   env->ski_active = true; // Comes before the TRACE so it gets the active stamp
    SKI_TRACE("ski_snapshot_reinitialize_cpu()\n");

    //SKI_TRACE("Hypercall: HYPERCALL_IO_TYPE_TEST_ENTER\n");
//    env->exception_index = -1;

	// Obsolete:
    //	env->ski_cpu.priority=-1;
    //	env->ski_cpu.priority_adjust=0;
    //	env->ski_cpu.blocked=true;

	// Restored from the snapshot:
	//	env->ski_cpu.nr_max_instr= hio.p.hio_test_enter.gh_nr_instr;
	//	env->ski_cpu.nr_instr_executed_other = 0;
	//	env->ski_cpu.nr_instr_executed = 0;
	//	env->ski_cpu.nr_cpus = hio.p.hio_test_enter.gh_nr_cpus;

	if(ski_global_init_preemmption>=0){
		env->ski_cpu.nr_max_instr = ski_global_init_preemmption;	
		SKI_TRACE("Initializing CPU with new value (env->ski_cpu.nr_max_instr=%d)\n",env->ski_cpu.nr_max_instr);
	}else{
		SKI_TRACE("Initializing CPU with existing value (env->ski_cpu.nr_max_instr=%d)\n",env->ski_cpu.nr_max_instr);
	}
	if(ski_init_options_nr_cpus){
		env->ski_cpu.nr_cpus = ski_init_options_nr_cpus;
		SKI_TRACE("Initializing CPU with new value (env->ski_cpu.nr_cpus=%d)\n",env->ski_cpu.nr_cpus);
	}else{
		SKI_TRACE("Initializing CPU with existing value (env->ski_cpu.nr_cpus=%d)\n",env->ski_cpu.nr_cpus);
	}

	SKI_TRACE("Writing to user-land: ski_init_options_seed: %d (ECX: %d, DS.base: %d)\n", ski_init_options_seed, env->regs[R_ECX], env->segs[R_DS].base);
	// Send to the guest the value of the the currente schedule seed
	if((ski_init_options_input_number[env->cpu_index])>-1){
		int tmp = ski_init_options_input_number[env->cpu_index];
		if (env->cpu_index == 0){
			tmp = tmp << 2;
			tmp += ski_init_options_cpu_1_mode;
		}
		else if (env->cpu_index == 1){
			tmp = tmp <<2;
			tmp += ski_init_options_cpu_2_mode;
		}
		hio.p.hio_test_enter.hg_res = tmp;
	}else{
		hio.p.hio_test_enter.hg_res = ski_init_options_seed;
	}
	int res = cpu_memory_rw_debug(env, env->regs[R_ECX] + env->segs[R_DS].base, &hio, sizeof(hio), 1);


	// Restored from the snapshot:
	//  env->ski_cpu.cr3 = env->cr[3];
    //	env->ski_cpu.gdt = env->gdt.base;
	//  env->ski_cpu.state = SKI_CPU_BLOCKED_ON_ENTER;

	// Obsolete?: 
	//  ski_initialize_interrupts(env);

	env->ski_cpu.nr_syscalls_self = 0;
	env->ski_cpu.nr_interrupts_self = 0;
	env->ski_cpu.nr_syscalls_other = 0;
	env->ski_cpu.nr_interrupts_other = 0;
	
	// Restored from the snapshot:
	//  env->ski_cpu.last_enter_cpu = 0;

	// Ignore:    ski_barrier(env, env->ski_cpu.nr_cpus, SKI_CPU_BLOCKED_ON_ENTER, SKI_CPU_RUNNING_TEST, EXCP_SKI_ENTER_WAITING, false);
	//  - changes ski_cpu.state but seems to be fixed when the last one unblocks it. and exception_index 
	ski_liveness_init(&env->ski_cpu.cpu_rs, SKI_MA_ENTRIES_MAX);
}



// Function called as soon a CPU initializes ski (but not on a resume from a snapshot)
void ski_first_in_test(CPUState* env){
    CPUState *penv = first_cpu;
	int nr_active = 0;

	// Need to check if only we're active
    while (penv) {
		if(penv->ski_active == true){
			nr_active++;
		}
        penv = (CPUState *)penv->next_cpu;
    }
	if(nr_active==1){
		SKI_TRACE("---------------------------- FIRST ------------------------------\n");

		ski_sched_instructions_current = 0;
		ski_sched_instructions_total = 0;
		ski_exec_trace_start(env);

		ski_apic_disable_timer = 1;
		ski_apic_disable_all = 1;
		//ski_disable_clock();
	}
}

// Function called after the enter barrier (when not resuming from a snapshot) and from the execution cycle (when resuming from a snapshot)
void ski_start_test(CPUState *env)
{
	ski_debug_print_config(env);
	ski_exec_trace_print_initial_comment(env);

	ski_threads_initialize(env);
	ski_threads_dump(env);
	
	ski_debug_memory(env);

	ski_nontesting_cpus_block(env);

	//TODO: Maybe put in a separate generic function to context switch?! 
    env->exception_index = EXCP_SKI_SCHEDULER;
	cpu_loop_exit(env);
}

// Fuction called after the last CPU finishes the test
// XXX: Check the explaination of the function
void ski_last_in_test(CPUState* env){
	//No need to test if we're last, it's assumed/guaranteed that we are the last
	SKI_TRACE("============================ LAST ==============================\n");

	// Reset the execution trace file
	ski_exec_trace_print_comment((char *)"Test finished");
	ski_exec_trace_flush();
	ski_exec_trace_stop(env);
	SKI_ASSERT(0);
}

void ski_reset_common(void){
	ski_apic_disable_timer = 0;
	ski_apic_disable_all = 0;
	//ski_enable_clock();
}

// Should be called just after ski_active goes to true for all  testing CPUs 
void ski_nontesting_cpus_block(CPUState *env){
	CPUState* penv = first_cpu;

	if(ski_block_other_cpus){
		SKI_TRACE("ski_nontesting_cpus_block: \n");
	    while (penv) {
		    if(penv->ski_active != true){
				// Save current state
				penv->ski_old_stop = penv->stop;
				penv->ski_old_stopped = penv->stopped;

				// Stop the CPUs
				penv->stop = 1;
				penv->stopped = 1;
		    }
			penv = (CPUState *)penv->next_cpu;
		}
	}
}

// Should be called just before ski_active goes to false for the testing CPUs 
// Called it only once per test (or remove assert...)
void ski_nontesting_cpus_unblock(CPUState *env){
	CPUState* penv = first_cpu;
	
	if(ski_block_other_cpus){
		SKI_TRACE("ski_nontesting_cpus_unblock: \n");
	    while (penv) {
			if(penv->ski_active != true){
				assert(penv->ski_old_stop != -1 && penv->ski_old_stopped != -1);

				// Restore the state of the CPUs
				penv->stop = penv->ski_old_stop;
				penv->stopped = penv->ski_old_stopped;

				// Reset the store values
				penv->ski_old_stop = -1;
				penv->ski_old_stopped = -1;
			}
			penv = (CPUState *)penv->next_cpu;
		}
	}
}

// -------------------------------------------------------------------------------------------------

void ski_deactivate_all_cpu(CPUState* env){
    CPUState *penv = first_cpu;
    SKI_TRACE("Deactivating ski for all CPUs\n");

	ski_nontesting_cpus_unblock(env);

    while (penv) {
		SKI_INFO("Deactivating ski for CPU %d\n", penv->cpu_index);
		penv->ski_active = false;
		bzero(&penv->ski_cpu,sizeof(penv->ski_cpu));
        penv = (CPUState *)penv->next_cpu;
    }
}

int ski_barrier(CPUState* env, int nr_cpus_barrier, int old_state, int new_state, int blocking_exception, bool call_sched){
    int nr_cpus_missing = nr_cpus_barrier;
    CPUState *penv = first_cpu;

    SKI_TRACE("BARRIER: Checking (blocking_exception: %d)\n", blocking_exception);
    while (penv) {
        if(penv->ski_cpu.state == old_state){
            nr_cpus_missing--;
        }
        SKI_TRACE("Cpu: %d, SKI active: %d, Testing state: %d\n", penv->cpu_index, penv->ski_active, penv->ski_cpu.state);
        penv = (CPUState *)penv->next_cpu;
    }

    SKI_TRACE("nr_cpus_missing: %d\n", nr_cpus_missing);

    if(nr_cpus_missing == 0){
        penv = first_cpu;
        SKI_TRACE("BARRIER: Unblocking all the other CPUs (blocking_exception: %d)\n", blocking_exception);
        while (penv) {
            if(penv->ski_cpu.state == old_state){
                penv->ski_cpu.state = new_state;
            }
            SKI_TRACE("Cpu: %d, SKI active: %d, Testing state: %d\n", penv->cpu_index, penv->ski_active, penv->ski_cpu.state);
            penv = (CPUState *)penv->next_cpu;
        }
    }else{
        //env->stop = 1;
        env->exception_index = blocking_exception;
        SKI_TRACE("Barrier: Waiting - CPU loop exit (blocking_exception: %d)\n", blocking_exception);
		cpu_loop_exit(env);
        SKI_TRACE("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX THIS SHOULD NO BE PRINTED XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
        // Does not return
        return 1; // If it were to reach here, it would be blocked
    }
    SKI_TRACE("BARRIER: Can make progress (blocking_exception: %d)\n", blocking_exception);
    // Ready to make progress
    return 0;
}


// -------------------------------------------------------------------------------------------------
void ski_threads_reset(CPUState *env){
	SKI_TRACE("ski_threads_reset();\n");
	ski_threads_no = 0;
}

void ski_threads_insert(CPUState *env,  int cpu, int i, int n){
    int reentry_no = 0;
    int entry_index = ski_threads_no;

	assert((i == -1) || (i>=0));
	assert(entry_index < MAX_SKI_THREADS);

    ski_tc_initialize(&ski_threads[entry_index], entry_index, entry_index, cpu, i, reentry_no);
    ski_threads[entry_index].count = n;
	
	ski_threads_no++;
}


static void ski_liveness_initialize(CPUState *env){
	int i;
	SKI_TRACE("ski_liveness_initialize(): initializing the fields for the liveness heuristics\n");

    for(i=0;i<ski_threads_no;i++){
		thread_context *tc = &ski_threads[i];
		tc->last_pause = -SKI_MAX_LIVENESS_PAUSE_DISTANCE;
		bzero(&tc->wait_rs,sizeof(ski_ma));
		tc->wait_for_rs_on_pause = 0;
		tc->wait_for_rs = 0;
	}

    CPUState *penv = first_cpu;
    while (penv) {
		penv->ski_cpu.wait_for_interrupt = 0;
		penv = (CPUState *)penv->next_cpu;
	}
}

void ski_config_priorities_parse_file(CPUState *env, char *filename, int seed);

static void ski_threads_initialize(CPUState *env){

	if (ski_init_options_priorities_filename[0]!=0){
		// XXX: Perhaps change the way the seed is calculated
		ski_config_priorities_parse_file(env, ski_init_options_priorities_filename, ski_init_options_seed);
		ski_liveness_initialize(env);	
		return;
	}
	SKI_TRACE("Need to specify the intial priorities with the ipfilterfile\n");
	assert(0);
	


	// DOES NOT REACH THIS POINT
    // BELLOW IS THE OLD HARDCODED INITIAL PRIORITIES
	// ALSO HAVE DISABLE THE FLIP MECHANISM



	// Simple way to define the initial priorities of threads (interrupts)
	int int_list_pre[]={231, 232, 233, 234, 235}; // High priority threads
	int int_list_in[] = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 234, 239}; // Medium priority
	int int_pre_count=3;

	int max_reentrancies = 1;
	int cpu_no, int_no, reentry_no, index, int_i;

	SKI_TRACE("ski_threads_initialize\n");
	memset(ski_threads, 0, sizeof(ski_threads));

	// TODO: Add an entry for catch-all other interrupts, add also to the other functions

	index = 0;
	cpu_no = 0;
    CPUState *penv = first_cpu;
	ski_threads_no = 0;
    while (penv) {
		if(penv->ski_active == true){
			assert(index<MAX_SKI_THREADS);
			cpu_no = penv->cpu_index;
			for(int_i=0; int_i<sizeof(int_list_pre)/sizeof(int); int_i++){
				int_no = int_list_pre[int_i];
				for(reentry_no = 0; reentry_no<max_reentrancies; reentry_no++){
					assert(index<MAX_SKI_THREADS);
					ski_tc_initialize(&ski_threads[index], index, index, cpu_no, int_no, reentry_no);
					ski_threads[index].count = int_pre_count;
					index++;
				}					
			}
		}
        penv = (CPUState *)penv->next_cpu;
	}

	penv = first_cpu;
    while (penv) {
		if(penv->ski_active == true){
			cpu_no = penv->cpu_index;

			assert(index<MAX_SKI_THREADS);
			ski_tc_initialize(&ski_threads[index], index, index, cpu_no, -1, -1);
			index++;

			for(int_i=0; int_i<sizeof(int_list_in)/sizeof(int); int_i++){
				int_no = int_list_in[int_i];
				for(reentry_no = 0; reentry_no<max_reentrancies; reentry_no++){
					assert(index<MAX_SKI_THREADS);
					ski_tc_initialize(&ski_threads[index], index, index, cpu_no, int_no, reentry_no);
					index++;
				}					
			}
		}
//		cpu_no++;
        penv = (CPUState *)penv->next_cpu;
    }
	ski_threads_no = index;
	ski_threads_current = 0;
	ski_threads_need_reevaluate = 0;

	// Temporary hack to flip the first cpu with the second cpu (which may not be CPU 0 and CPU 1)
	/*
	int flip_cpu_01 = (env->ski_cpu.nr_max_instr > SKI_SCHED_SEED_FLIP) ? 1 : 0;
	if(flip_cpu_01){
		int i;
		int flip_first_cpu = -1;
		int flip_second_cpu = -1;
	    for(i=0;i<ski_threads_no;i++){
			thread_context *tc = &ski_threads[i];
			if(tc->exists){
				if(flip_first_cpu < 0){
					flip_first_cpu = tc->cpu_no;
				}else if(tc->cpu_no != flip_first_cpu){
					flip_second_cpu = tc->cpu_no;
					break;
				}
			}
		}

		assert((flip_first_cpu >= 0) && (flip_second_cpu >= 0) && (flip_first_cpu != flip_second_cpu));

	    for(i=0;i<ski_threads_no;i++){
			thread_context *tc = &ski_threads[i];
			if(tc->exists){
				int current_cpu = tc->cpu_no;
				if(current_cpu == flip_first_cpu){
					tc->cpu_no = flip_second_cpu;
				}else if(current_cpu == flip_second_cpu){
					tc->cpu_no = flip_first_cpu;
				}
			}
		}
	}
	*/

}

static CPUState* find_cpu(int cpu_no){
    CPUState *penv = first_cpu;
    while (penv) {
		if( penv->cpu_index == cpu_no){
			return penv;
		}
        penv = (CPUState *)penv->next_cpu;
	}
	assert(0);
	return 0;
}

static inline int ski_threads_calculate_priority(CPUState *env, thread_context* tc){
	int priority = tc->priority + tc->priority_adjust;
	if(unlikely((tc->is_int==0) && find_cpu(tc->cpu_no)->ski_cpu.wait_for_interrupt)){
		priority += SKI_LIVENESS_PRIORITY_HLT; 
	}
	if(unlikely(tc->wait_for_rs_on_pause)){
		priority += SKI_LIVENESS_PRIORITY_PAUSE;
	}
	if(unlikely(tc->wait_for_rs)){
		priority += SKI_LIVENESS_PRIORITY_NOPAUSE_LOOP;
	}

	//SKI_TRACE("ski_threads_calculate_priority: thread->id: %d cpu_index: %d is_int: %d priority: %d\n", tc->id, tc->cpu_no, tc->is_int, priority);
	return priority;
}


int ski_cpus_dump(CPUState *env){
    CPUState *penv = first_cpu;
	//SKI_TRACE_ACTIVE("ski_cpus_dump:\n");
	char eips_str[512];
	strcpy(eips_str,"");
	while (penv) {
		char tmp[128];
		sprintf(tmp, "%d: 0x%x ", penv->cpu_index, penv->eip);
		strcat(eips_str, tmp);
		penv = (CPUState *)penv->next_cpu;
	}
	SKI_TRACE_ACTIVE("ski_cpus_dump: %s\n", eips_str);
	return 0;
}

//#define SKI_TRACE_FIND_NEXT_RUNNABLE(x,...) SKI_TRACE_ACTIVE(x,...)  // Print log messages
#define SKI_TRACE_FIND_NEXT_RUNNABLE(x,...) while(0){}  // Reduce the verbosity

#define SKI_TRACE_INTERRUPTS(...) SKI_TRACE_ACTIVE( __VA_ARGS__ );  // Print log messages
//#define SKI_TRACE_INTERRUPTS(x,...) while(0){}  // Reduce the verbosity


static thread_context* ski_threads_find_next_runnable(CPUState *env){
	SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable\n");
	int i;
	int alive_t;
	int count;

	thread_context *res = NULL;
	count = 0;
	char debug_str[256];

	for(i=0;i<ski_threads_no;i++){
		thread_context *tc = &ski_threads[i];
		alive_t = 0;
		if(tc->exists){
			count+=1;
			alive_t++;
		//{ && tc->state==SKI_TC_STATE_RUNNABLE){
//			SKI_TRACE_ACTIVE("ski_threads_find_next_runnable: checking thread->id: %d\n", tc->id);
			if(tc->wait_for_rs || tc->wait_for_rs_on_pause || (tc->is_int == 0 && find_cpu(tc->cpu_no)->ski_cpu.wait_for_interrupt)){
				SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable: checking thread->id: %d => Waiting [hlt: %d, pause: %d, nopase_rs: %d] (IGNORING)\n", 
											tc->id, find_cpu(tc->cpu_no)->ski_cpu.wait_for_interrupt, tc->wait_for_rs_on_pause, tc->wait_for_rs);
				continue;
			}

			if(tc->is_int==0 && find_cpu(tc->cpu_no)->ski_cpu.state == SKI_CPU_BLOCKED_ON_EXIT){
				// The CPU threads that have reached the exit barrier and therefore the CPU thread is not not runnable (but the interrupt threads may still be)
				//TODO: Maybe update the SKI_TC_STATE_RUNNABLE instead?
				SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable: checking thread->id: %d => End of test and not interrupt (IGNORING)\n", tc->id);
				continue;
			}
			if(tc->is_int == 1){
				// Interrupt thread
				if(ski_interrupts_is_top_hw_stack(env, tc)){
					// Interrupt thread is running on that CPU
					SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable: checking thread->id: %d (calc: %d) => Int and top of stack (CONSIDERING)\n", tc->id, ski_threads_calculate_priority(env,tc));
				
				}else if(ski_cpu_can_start_interrupts(find_cpu(tc->cpu_no))==0){
					// Interrupt thread is not startable so: If the CPU is not able to run interrupts skip interrupt threads for that CPU
					SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable: checking thread->id: %d => Int, not top of stack, and not able to start (IGNORING)\n", tc->id);
					continue;			
				}
				SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable: checking thread->id: %d (calc: %d) => Int, not top of stack but can start (CONSIDERING)\n", tc->id, ski_threads_calculate_priority(env,tc));
			}else{
				// CPU thread
				if(ski_interrupts_in_hw_int(env, find_cpu(tc->cpu_no))){
					// CPU thread is not running on that CPU (ie., there's an interrupt thread running)
					SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable: checking thread->id: %d => CPU, not top of stack (IGNORING)\n", tc->id);
					continue;
				}
				SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable: checking thread->id: %d (calc: %d) => CPU, top of stack (CONSIDERING)\n", tc->id, ski_threads_calculate_priority(env,tc));
			}
		
			res = (res && (ski_threads_calculate_priority(env,res) < ski_threads_calculate_priority(env,tc)))? res : tc;
			//sprintf(debug_str, "res(thread_context) updated || priority(res%d)=%d priority(tc%d)=%d, tc=%08X\n", 
			//	res->id, ski_threads_calculate_priority(env,res), tc->id, ski_threads_calculate_priority(env,tc), tc);
			//ski_exec_trace_print_comment(debug_str);
		}
	}
	//printf("%d threads exist ski_threads_no is %d \nn", alive_t, ski_threads_no);
	if (alive_t == 0){
		//printf("no threads at all, otherwise it's because of the priority thing\n");
	}
	if(res){
		SKI_TRACE_FIND_NEXT_RUNNABLE("ski_threads_find_next_runnable: Next runnable thread id = %d, cpu_no = %d, is_int = %d, int_no = %d\n", res->id, res->cpu_no, res->is_int, res->int_no);
		//sprintf(debug_str, "count %d ski_threads_find_next_runnable: Next runnable thread id = %d, cpu_no = %d, is_int = %d, int_no = %d\n", count, res->id, res->cpu_no, res->is_int, res->int_no);
		//ski_exec_trace_print_comment(debug_str);
		if(ski_threads_calculate_priority(env,res)>SKI_LIVENESS_PRIORITY_NOPAUSE_LOOP){
			ski_tc_dump_all(env);
			ski_interrupts_dump_stack_all(env);
			SKI_TRACE_ACTIVE("ski_threads_find_next_runnable: ERROR - priority is beyond threshold...something is probably wrong (some interrupt missing??)!!!\n");
			
			SKI_ASSERT_MSG(0, SKI_EXIT_NO_RUNNABLE, "RUNNABLE_THREAD(priority_issue)");
			assert(0);
		}
	}else{
		ski_tc_dump_all(env);
		ski_interrupts_dump_stack_all(env);
		SKI_TRACE_ACTIVE("ski_threads_find_next_runnable: ERROR - no runnable thread found!!!\n");
		ski_cpus_dump(env);
	//	SKI_STATS_ASSERT_LOG(0,"ASSERT_RUNABLE");
		SKI_ASSERT_MSG(0, SKI_EXIT_NO_RUNNABLE, "RUNNABLE_THREAD");
		assert(0);
		SKI_ASSERT(0);
	}
	//ski_tc_dump_all(env);
	//ski_interrupts_dump_stack_all(env);
	return res;
}

int ski_threads_find_next_runnable_int(CPUState *env){
	int res;
	//thread_context *tc = ski_threads_find_next_runnable(env);
	thread_context *tc = ski_threads_current;
	if(!tc || (tc->is_int == 0) || (tc->cpu_no != env->cpu_index)){
		SKI_TRACE_ACTIVE("ski_threads_find_runnable_int: -1\n")
		return -1;
	}

	res = tc->int_no;
	SKI_TRACE_ACTIVE("ski_threads_find_runnable_int: int_no = %d\n", res)
	return res;
}

// If exists == -1 match any exists
static thread_context* ski_threads_find(CPUState *env, int exists, int cpu_no, int is_int, int int_no, int reentry_no, int state_mask){
    int i;
	thread_context * res = NULL;

	for(i=0;i<ski_threads_no;i++){
        thread_context *tc = &ski_threads[i];
        if((exists == -1 || tc->exists==exists) && 
		   (state_mask & tc->state)){
            if (tc->cpu_no == cpu_no && tc->is_int == is_int && tc->int_no == int_no){
				res = (res && (ski_threads_calculate_priority(env, res) < ski_threads_calculate_priority(env, tc))) ? res : tc;
				/*
				// XXX: Ignore the reeentry_no ("If reentry_no == -1 find the one with the maximum reentry_no")
				if(reentry_no==-1){
					res = (res && res->reentry_no > tc->reentry_no)? res : tc;
                }else if(tc->reentry_no == reentry_no){
                    res = tc;
                }*/
            }
        }  
    } 
	SKI_TRACE_ACTIVE("    ski_threads_find: found id = %d  (exists = %d, cpu_no = %d, is_int = %d, int_no = %d, reentry_no = %d)\n", 
	   (res==NULL)?-1:res->id, exists, cpu_no, is_int, int_no, reentry_no);   
	/*if(1 || !exists){
		ski_tc_dump_all(env);
	}*/
	return res;
}

void ski_handle_new_irq_set(CPUState*env, int vector_num, int trigger_mode){
	if(!env->ski_active){
		return;
	}

	int old_ski_trace_active=ski_trace_active;

	if(vector_num == 239 || vector_num == 48){
		// Reduce verbosity for these interrupts
		ski_trace_active = false;
	}

	SKI_TRACE_ACTIVE("ski_handle_new_irq_set: vector_num = %d, trigger_mode = %d\n", vector_num, trigger_mode);
	int cpu_no = env->cpu_index;
	int next_reentry = -1;

	//SKI_TRACE_ACTIVE("ski_handle_new_irq_set: finding whether there is already this specific interrupt (for the specific CPU and reentrancy level)\n");    

	// XXX: Ignore reentrancies for now
	//thread_context * tc_max_reentry = NULL;
	//tc_max_reentry = ski_threads_find(env, 1, cpu_no, 1, vector_num, -1);
	//next_reentry = (tc_max_reentry)? tc_max_reentry->reentry_no + 1: 0;

	next_reentry = 1;

	// Try to find a thread in the thread list to spawn (exists=0 -> exist=1)
    // This thread should have the same CPU, and int_no and the highest priority (not "lowest available reentry_no" anymore)
	// Also need to update the count (ie., the max number os spawns still allowed)
	thread_context * tc_new_interrupt = NULL;
	tc_new_interrupt = ski_threads_find(env, 0, cpu_no, 1, vector_num, next_reentry, SKI_TC_STATE_NA);
	if(tc_new_interrupt == NULL){
		// Such thread does not exist in the thread list (i.g., int_no that the developer does not care about, or reentry_no that the developer does not care about)
		tc_new_interrupt = ski_threads_find(env, 0, cpu_no, 1, vector_num, next_reentry, SKI_TC_STATE_COUNT_ZERO);
		if(tc_new_interrupt){
			SKI_TRACE_ACTIVE_AND_EXEC("ski_handle_new_irq_set: tc_new_interrupt->count <= 0 (skipping this interrupt) [cpu_no = %d, int_no = %d]\n", cpu_no, vector_num);
		}else{
			SKI_TRACE_ACTIVE_AND_EXEC("ski_handle_new_irq_set: tc_new_interrupt == NULL (skipping this interrupt) [cpu_no = %d, int_no = %d]\n", cpu_no, vector_num);
		}
	}else if(tc_new_interrupt->count<=0){
		// XXX: We don't reach this case because we're filtering for threads in the state _NA
		assert(0);
		// Such thread was already spawned enough times during this execution
		//SKI_TRACE_ACTIVE("ski_handle_new_irq_set: tc_new_interrupt->count <= 0 (skipping this interrupt) [cpu_no = %d, int_no = %d]\n", cpu_no, vector_num);
	}else{
		// Found the thread: spawned it!
		SKI_TRACE_ACTIVE("ski_handle_new_irq_set: spawning this interrupt [cpu_no = %d, int_no = %d]\n", cpu_no, vector_num);
		tc_new_interrupt->exists = 1;
		tc_new_interrupt->state = SKI_TC_STATE_NEEDS_TRIGGER;
		tc_new_interrupt->count--;
	}
	ski_trace_active = old_ski_trace_active;
}

void ski_handle_cpu_begin(CPUState *env){
	int in_hw_int =	ski_interrupts_in_hw_int(env, env);
	thread_context *tc = ski_threads_current;
	int expect_hw_int = tc->is_int;

	//SKI_TRACE_ACTIVE("ski_handle_cpu_begin (env->cpu_index = %d, in_hw_int = %d, expecting int = %d)\n", env->cpu_index, in_hw_int, expect_hw_int);
	if(in_hw_int != expect_hw_int){
		ski_debug();
	}
	SKI_ASSERT(in_hw_int == expect_hw_int);

}

void ski_handle_interrupt_begin(CPUState * env, int int_no, int is_int_instruction, int is_hw){
	SKI_TRACE_ACTIVE("ski_handle_interrupt_begin (env->cpu_index = %d, int_no = %d, is_int_instruction = %d, is_hw = %d)\n", env->cpu_index, int_no, is_int_instruction, is_hw);
	int reentry_no = 0;

	int i;
	for(i=0;i<SKI_MAX_INTERRUPTS_STACK;i++){
		ski_interrupt* si = &env->ski_cpu.interrupts_stack[i];
/*		if((si->exists == 1) && (si->int_no == int_no) && (si->is_hw == is_hw)){
			reentry_no++;
			SKI_TRACE("not implemented\n");
			assert(0);
			//XXX: Probably need to reevaluate, to return and the _end() function needs to match this also ???
		}
*/		if(si->exists == 0){
			SKI_TRACE_ACTIVE("ski_handle_interrupt_begin: adding entry i = %d\n", i);
			si->exists = 1;
			si->int_no = int_no;
			si->is_int_instruction = is_int_instruction;
			si->is_hw = is_hw;
			env->ski_cpu.interrupts_stack_no++;

			// If it's HW int, confirm that it was waiting for the trigger and that it has occured
			if(is_int_instruction == 0 && is_hw == 1){
				thread_context* tc = ski_threads_find(env, 1,  env->cpu_index, 1, int_no, reentry_no, SKI_TC_STATE_NEEDS_TRIGGER);
				assert(tc);
				SKI_ASSERT(tc);
				SKI_ASSERT(tc->state == SKI_TC_STATE_NEEDS_TRIGGER);
				tc->state = SKI_TC_STATE_RUNNABLE;
				char debug_str[128];
				sprintf(debug_str, "Dispatching interrupt %d on CPU %d", tc->int_no, tc->cpu_no);
				if (ski_init_options_trace_instructions_enabled)
					ski_exec_trace_print_comment(debug_str);
				if(!ski_threads_current || ski_threads_current->id != tc->id){
					ski_tc_dump_all(env);
					ski_interrupts_dump_stack_all(env);
					SKI_TRACE_ACTIVE("ERROR: !ski_threads_current || ski_threads_current->id != tc->id\n");
					ski_cpus_dump(env);
					//SKI_STATS_ASSERT_LOG(0,"ASSERT_INTERRUPT");
					SKI_ASSERT_MSG(0, SKI_EXIT_ASSERT_INTERRUPT, "INTERRUPT");
					assert(0);
					SKI_ASSERT(0);
				}
			}

			ski_threads_need_reevaluate = 1; //XXX: Optimize: Only need to reevaluate if it's a hw & non-int interrupt
			//ski_debug();
			return;
		}
	}
	SKI_TRACE_ACTIVE("ski_handle_interrupt_begin: unable to find an available spot\n");
	SKI_ASSERT(0);
	assert(0);
}


void ski_handle_interrupt_end(CPUState *env){
	SKI_TRACE_ACTIVE("ski_handle_interrupt_end (env->cpu_index = %d)\n", env->cpu_index);

	if(env->ski_cpu.interrupts_stack_no>0){
		env->ski_cpu.interrupts_stack_no--;
		ski_interrupt* si = &env->ski_cpu.interrupts_stack[env->ski_cpu.interrupts_stack_no];
		if(si->is_hw){
			thread_context *tc = ski_threads_find(env, 1, env->cpu_index, 1, si->int_no, -1, SKI_TC_STATE_RUNNABLE);  // with maximum reeentry
			assert(tc);
			SKI_TRACE_ACTIVE("ski_handle_interrupt_end: disabling tc->id = %d\n", tc->id);
			tc->exists = 0;
			if(tc->count == 0){
				tc->state = SKI_TC_STATE_COUNT_ZERO;
			}else{
				tc->state = SKI_TC_STATE_NA;
			}

			if(env->ski_cpu.wait_for_interrupt){
				SKI_TRACE("Clearing the wait_for_interrupt (caused by a previous hlt instruction)\n")

    	        char debug_str[256];
	            assert(ski_threads_current);
           		sprintf(debug_str, "Waking up because of interrupt (cpu_no: %d) [by thread %d]", env->cpu_index, tc->id);
				if (ski_init_options_trace_instructions_enabled)
        	    	ski_exec_trace_print_comment(debug_str);
	
				env->ski_cpu.wait_for_interrupt = 0;
			}

			ski_threads_need_reevaluate = 1;
		}
		si->exists = 0;
		//ski_debug();
	}else{
		SKI_ASSERT(0);
	}
}

void ski_threads_reevaluate(CPUState *env){
	ski_threads_need_reevaluate = 1;
}

void ski_interrupts_dump_stack_all(CPUState* env){
	CPUState *penv = first_cpu;
	while(penv){
		ski_interrupts_dump_stack(penv);
		penv = (CPUState *)penv->next_cpu;
	}
}

void ski_interrupts_dump_stack(CPUState* env){
	int i;
/*	
	SKI_TRACE_ACTIVE("ski_interrupts_dump_stack\n");
	for (i=0;i<SKI_MAX_INTERRUPTS_STACK;i++){
		ski_interrupt* si = &env->ski_cpu.interrupts_stack[i];
		if(si->exists == 0){
			SKI_TRACE_ACTIVE("ski_interrupts_dump_stack: [%d] exists = %d\n", i, si->exists);
		}else{
			SKI_TRACE_ACTIVE("ski_interrupts_dump_stack: [%d] exists = %d, int_no = %d, is_int_instruction = %d, is_hw = %d\n", i, si->exists, si->int_no, si->is_int_instruction, si->is_hw);
		}		
	}
*/
	// Minimalist debugging version
	{
		char str[4*1024];
		str[0]=0;

		sprintf(str+strlen(str), "ski_interrupts_dump_stack - ALL [i, exits, int_no, is_int_instruction, is_hw]: \n");
		for (i=0;i<SKI_MAX_INTERRUPTS_STACK;i++){
			ski_interrupt* si = &env->ski_cpu.interrupts_stack[i];
			if(si->exists == 0){
				sprintf(str+strlen(str), "[%d, %d] ", i, si->exists);
			}else{
				sprintf(str+strlen(str), "[%d, %d, %d, %d, %d] ", i, si->exists, si->int_no, si->is_int_instruction, si->is_hw);
			}		
		}
		sprintf(str+strlen(str),"\n");
		SKI_TRACE_ACTIVE(str);	
	}
}

int ski_interrupts_in_hw_int(CPUState* env, CPUState* penv){
	int i;
	
	for(i=0;i<SKI_MAX_INTERRUPTS_STACK;i++){
		ski_interrupt* si = &penv->ski_cpu.interrupts_stack[i];
		if(si->exists && si->is_hw==1 && si->is_int_instruction==0){
//			SKI_TRACE_ACTIVE("ski_interrupts_in_hw_int: CPU = %d in HW int (found HW entry i = %d)\n", penv->cpu_index, i);
			return  1;
		}
	}
//	SKI_TRACE_ACTIVE("ski_interrupts_in_hw_int: CPU = %d not in HW int\n", penv->cpu_index);
	return 0;
}

static int ski_interrupts_is_top_hw_stack(CPUState*env, thread_context *tc){
	int i;
	//SKI_TRACE_ACTIVE("ski_interrupts_is_top_hw_stack\n");
	int reentry_no = -1;
	CPUState *penv = find_cpu(tc->cpu_no);

	for(i=0;i<=SKI_MAX_INTERRUPTS_STACK;i++){
		ski_interrupt* si = &penv->ski_cpu.interrupts_stack[i];
		if(si->exists && si->is_hw==1 && si->is_int_instruction==0){
			if(si->int_no == tc->int_no){
				// We only care about HW interrrupts
//				SKI_TRACE_ACTIVE("ski_interrupts_is_top_hw_stack: tc->id = %d (found HW entry i = %d)\n", tc->id, i);
				reentry_no++;
			}
		}
	}
//	SKI_TRACE_ACTIVE("ski_interrupts_is_top_hw_stack: tc->id = %d, int_no = %d, tc->reentrancy = %d, reentrancy = %d\n", tc->id, tc->int_no, tc->reentry_no, reentry_no);
	if(tc->reentry_no == reentry_no){
		return 1;
	}
	return 0;
}


static int ski_cpu_can_start_interrupts(CPUState* env){
	if ((env->interrupt_request & CPU_INTERRUPT_HARD) &&
	  (((env->hflags2 & HF2_VINTR_MASK) &&
		 (env->hflags2 & HF2_HIF_MASK)) ||
		(!(env->hflags2 & HF2_VINTR_MASK) &&
		 (env->eflags & IF_MASK &&
		  !(env->hflags & HF_INHIBIT_IRQ_MASK))))
		  ) {
//	   &&(!env->ski_active || apic_irq_pending(env->apic_state)!=0)
		return 1;
	}
	return 0;
}

int ski_threads_adjust_priority_current(CPUState* env)
{
    if((ski_sched_instructions_current>=0) && (ski_sched_instructions_current % SKI_SCHED_ADJUST_MODULO) == 0){
		assert(ski_threads_current);
		int new_adjust = ski_threads_current->priority_adjust + SKI_SCHED_ADJUST_INC;
        SKI_TRACE("ski_threads_adjust_priority_current: ski_sched_instructions_current = %d, old adjustment = %d, new adjustment = %d (EIP: %x)\n", 
				ski_sched_instructions_current, ski_threads_current->priority_adjust, new_adjust,  env->eip);
		
		char debug_str[256];
		assert(ski_threads_current);
		sprintf(debug_str, "Priority adjustment (cpu_no: %d, int_no: %d, tc->id: %d, old adjustment: %d new adjustment: %d)", 
							ski_threads_current->cpu_no, ski_threads_current->int_no, ski_threads_current->id, ski_threads_current->priority_adjust, new_adjust);
		ski_exec_trace_print_comment(debug_str);
		if (ski_forkall_enabled)
			SKI_ASSERT_MSG(new_adjust < SKI_DEBUG_MAX_ADJUST, SKI_EXIT_MAX_PRIORITY_ADJ, "MAX_PRIORITY_ADJ");
		//assert(new_adjust < SKI_DEBUG_MAX_ADJUST);


		if(ski_init_options_heuristics_statistics_enabled){
			ski_heuristics_statistics_print(SKI_HEURISTICS_STATISTICS_STARVATION, env->eip, ski_threads_current->cpu_no, ski_exec_instruction_counter_total);
		}

        ski_threads_current->priority_adjust = new_adjust;
		ski_threads_need_reevaluate = 1;
        return 1;
    }
    return 0;
}

int ski_threads_all_wake_rs(CPUState *env, int mem_address)
{
	int found = 0;
	int i;

    for(i=0;i<ski_threads_no;i++){
		thread_context *tc = &ski_threads[i];
/*		if(unlikely(tc->exists && 
		           (tc->wait_for_rs_on_pause || tc->wait_for_rs) && 
				 	ski_liveness_is_present(&tc->wait_rs, mem_address))){*/
		/* Branching was mispredicted here... */
		if(unlikely(tc->wait_for_rs_on_pause || tc->wait_for_rs) &&
				tc->exists && 
				ski_liveness_is_present(&tc->wait_rs, mem_address)){
			SKI_TRACE("ski_threads_all_wake_rs: found waiting thread %d\n", tc->id);

			char debug_str[256];
			assert(ski_threads_current);
			sprintf(debug_str, "Waking up because of RS (cpu_no: %d, int_no: %d, tc->id: %d, is_pause: %d, mem_address: %x) [by thread %d]", 
								tc->cpu_no, tc->int_no, tc->id, tc->wait_for_rs_on_pause, mem_address, ski_threads_current->id);
			if (ski_init_options_trace_instructions_enabled)
				ski_exec_trace_print_comment(debug_str);

			found = 1;
			tc->wait_for_rs_on_pause = 0;
			tc->wait_for_rs = 0;
			ski_liveness_reset(&tc->wait_rs);
		}
	}

	if(found){
		ski_threads_need_reevaluate = 1;
		return 1;
	}
	return 0;
}

void ski_threads_self_wait_rs(CPUState *env, int is_pause)
{
	thread_context *tc = ski_threads_current;
	assert(tc);
	SKI_TRACE("ski_threads_wait_rs (cpu_no: %d, int_no: %d, tc->id: %d, is_pause: %d)\n", tc->cpu_no, tc->int_no, tc->id, is_pause);

    char debug_str[256];
	sprintf(debug_str, "Waiting on RS (cpu_no: %d, int_no: %d, tc->id: %d, is_pause: %d)", tc->cpu_no, tc->int_no, tc->id, is_pause);
	if (ski_init_options_trace_instructions_enabled)
		ski_exec_trace_print_comment(debug_str);

	if(is_pause){
		tc->wait_for_rs_on_pause = 1;
	}else{
// TODO: Disabled for now....
		tc->wait_for_rs = 1;
	}

	if(ski_init_options_heuristics_statistics_enabled){
		if(is_pause){
			ski_heuristics_statistics_print(SKI_HEURISTICS_STATISTICS_PAUSE, env->eip, tc->cpu_no, ski_exec_instruction_counter_total);
		}else{
			ski_heuristics_statistics_print(SKI_HEURISTICS_STATISTICS_LOOP, env->eip, tc->cpu_no, ski_exec_instruction_counter_total);
		}
	}

	tc->last_pause = -SKI_MAX_LIVENESS_PAUSE_DISTANCE;

	ski_liveness_copy(&tc->wait_rs, &env->ski_cpu.cpu_rs);
	ski_liveness_dump(&tc->wait_rs);

	ski_threads_need_reevaluate = 1;
}

void ski_threads_self_wait_hlt(CPUState *env)
{
	SKI_TRACE("ski_threads_self_wait_hlt\n");

    char debug_str[256];
	thread_context* tc = ski_threads_current;
	assert(tc);
	sprintf(debug_str, "Waiting on HLT (cpu_no: %d, int_no: %d, tc->id: %d)", tc->cpu_no, tc->int_no, tc->id);
	if (ski_init_options_trace_instructions_enabled)
		ski_exec_trace_print_comment(debug_str);
	
	if(ski_init_options_heuristics_statistics_enabled){
		ski_heuristics_statistics_print(SKI_HEURISTICS_STATISTICS_HLT, env->eip, tc->cpu_no, ski_exec_instruction_counter_total);
	}

    ski_liveness_dump(&env->ski_cpu.cpu_rs);

// TODO: Disabled for now....
	env->ski_cpu.wait_for_interrupt = 1;
	ski_threads_need_reevaluate = 1;
}

// Reduces the priority of the current CPU (i.e, increases the priority value)
// assign the current tc with lowest priority

void ski_threads_invert_priority(CPUState * env)
{
	thread_context* tc = ski_threads_current;
	int new_priority;
	int lowest_priority = 0;
	int i;
	int compare_count = 0;
	char debug_str[512];
	SKI_TRACE("ski_threads_invert_priority\n");

	assert(tc);

    for(i=0;i<ski_threads_no;i++){
        thread_context *tc = &ski_threads[i];
        if(tc->exists && 
		  (tc->int_no < 0 || tc->state != SKI_TC_STATE_NEEDS_TRIGGER) &&
		  (!(tc->wait_for_rs || tc->wait_for_rs_on_pause || find_cpu(tc->cpu_no)->ski_cpu.wait_for_interrupt)) ){
			compare_count++;
			lowest_priority = MAX(lowest_priority , tc->priority);
        }
    }
	new_priority = lowest_priority + 1;
	SKI_TRACE("ski_threads_invert_priority: Num_thread %d Preemption point (EIP: %x) - changing priority of thread %d from %d to %d\n", ski_threads_no, env->eip, tc->id, tc->priority, new_priority);
	sprintf(debug_str, "ski_threads_invert_priority: Num_thread %d Preemption point (EIP: %x) - changing priority of thread %d from %d to %d compare_t %d", ski_threads_no, env->eip, tc->id, tc->priority, new_priority, compare_count);
	ski_exec_trace_print_comment(debug_str);	
	printf("ski_threads_invert_priority: Num_thread %d Preemption point (EIP: %x) - changing priority of thread %d from %d to %d compare_t %d\n", ski_threads_no, env->eip, tc->id, tc->priority, new_priority, compare_count);
	tc->priority = new_priority;
	ski_threads_need_reevaluate = 1;
}


extern int ski_init_options_preemption_mode;

int ski_preemption_mode_matches(CPUState *env);

int ski_preemption_mode_matches(CPUState *env){
	if(!ski_threads_current){
		return 1;
	}

	if(ski_init_options_preemption_mode == SKI_PREEMPTION_MODE_ALL){
		return 1;
	}

	if((ski_init_options_preemption_mode == SKI_PREEMPTION_MODE_CPU) && (ski_threads_current->int_no <0)){
		return 1;
	}

	if((ski_init_options_preemption_mode == SKI_PREEMPTION_MODE_CPU_AND_NONTIMER) && ((ski_threads_current->int_no <0) || ((ski_threads_current->int_no != 239) && (ski_threads_current->int_no != 48)))){
		return 1;
	}

	return 0;
}


// This function also has side-effects (writes to logs and updates the ski_threads_current)
int ski_cpu_can_run(CPUState *env){
	int res = 1;

	if(env->ski_active == 1 && env->ski_cpu.state == SKI_CPU_BLOCKED_ON_ENTER){
		// Wait on barrier: SKI is not yet ready
		res = 0;
	}

	if(env->ski_active == 1 && (env->ski_cpu.state == SKI_CPU_RUNNING_TEST || env->ski_cpu.state == SKI_CPU_BLOCKED_ON_EXIT)){
		// If test is in progress
		//ski_debug();
		thread_context *tc = ski_threads_find_next_runnable(env);
		if(tc==0 || tc->cpu_no == env->cpu_index){
			// If we should run a thread in this CPU
			res = 1;
			char trace_comment[256];
			char trace_comment_nl[256];
			char trace_simple_comment[256];
			if(tc && (ski_threads_current != tc)){
				ski_cpus_dump(env);

				
				sprintf(trace_comment,"Switching to thread %d, CPU %d [%d, %d] from thread %d, CPU %d [%d, %d]", 
					tc->id, 
					tc->cpu_no, 
					tc->priority, 
					tc->priority_adjust,
					ski_threads_current ? ski_threads_current->id : -1,
					ski_threads_current ? ski_threads_current->cpu_no : -1,
					ski_threads_current ? ski_threads_current->priority : -1,
					ski_threads_current ? ski_threads_current->priority_adjust : -1);
				
				sprintf(trace_simple_comment,"Executing CPU %d, int %d (thread %d, calc %d)", 
					tc->cpu_no, tc->int_no, tc->id, ski_threads_calculate_priority(env, tc)); 

				sprintf(trace_comment_nl,"%s\n",trace_comment);
				
				ski_loop_flush();
				ski_sched_instructions_current=0;
				ski_exec_trace_print_comment(trace_comment);
				if (ski_init_options_trace_instructions_enabled)
					ski_exec_trace_print_comment(trace_simple_comment);

				ski_liveness_reset(&env->ski_cpu.cpu_rs);

				SKI_TRACE(trace_comment_nl);
				//ski_threads_dump(env);
				//ski_interrupts_dump_stack(env);
				ski_threads_current = tc;
			}
			else{
				//sprintf(trace_comment, "Sticking to the current thread %d, CPU %d", tc->id, tc->cpu_no);
				//ski_exec_trace_print_comment(trace_comment);
			}
		}else{
			//char trace_comment[256];
			//sprintf(trace_comment, "Cannot switching threads tc:%08x tc->cpu_no:%d env->cpu:%d", tc, tc->cpu_no, env->cpu_index);
			//ski_exec_trace_print_comment(trace_comment);
			res = 0;
		}
		ski_threads_need_reevaluate = 0;
	}

	/*
	const char *state_str = "???";
	switch(env->ski_cpu.state){
		case SKI_CPU_BLOCKED_ON_ENTER: state_str = "SKI_CPU_BLOCKED_ON_ENTER"; break;
		case SKI_CPU_BLOCKED_ON_EXIT: state_str = "SKI_CPU_BLOCKED_ON_EXIT"; break;
		case SKI_CPU_RUNNING_TEST: state_str = "SKI_CPU_RUNNING_TEST"; break;
		default: state_str = "??"; break;
	}
	*/
	//SKI_TRACE_ACTIVE("ski_cpu_can_run (res = %s, env->exception_index = %d, ski_cpu.state = %d [%s], current_thread->id: %d)\n", (res==1?"CAN_RUN":"CANNOT_RUN"), env->exception_index, env->ski_cpu.state, state_str, ski_threads_current?ski_threads_current->id:0);
	if(env->ski_active == 1 && ski_threads_current){
		//ski_tc_dump(env,ski_threads_current);
	}
	return res;
}

// Function that runs on every cycle and checks which are the active interrups for the running thread
static void ski_thread_enable_interrupt(CPUState *env){
	SKI_TRACE_ACTIVE("ski_thread_enable_interrupt\n");
				
}

void ski_threads_dump(CPUState* env){
	int i;
	/*
	SKI_TRACE("ski_threads_dump:\n");
	for(i=0;i<ski_threads_no;i++){
		thread_context *tc = &ski_threads[i]; 
		//if(tc->exists){
			ski_tc_dump(env, tc);
		//}
	}
	*/
	// Minimalist debugging
	{
		char str[4*1024];
		str[0]=0;

		sprintf(str+strlen(str), "ski_threads_dump: - ALL [id, e, pri, adj, CPU, is_int, int_no, re_no, count, can_st_int, state]: \n");
		for(i=0;i<ski_threads_no;i++){
			thread_context *tc = &ski_threads[i]; 
			if(tc->exists == 0){
			//	sprintf(str+strlen(str), "[%d, %d] ",
			//		tc->id,
			//		tc->exists);

			}else{
				char *state;
				switch(tc->state){
					case SKI_TC_STATE_RUNNABLE: state = "RUN"; break;
					case SKI_TC_STATE_BLOCKED: state = "BLO"; break;
					case SKI_TC_STATE_IS_WAITING: state = "ISW"; break;
					case SKI_TC_STATE_MAYBE_WAITING: state = "MAY"; break;
					case SKI_TC_STATE_NA: state = "NA_"; break;
					case SKI_TC_STATE_NEEDS_TRIGGER: state = "NED"; break;
					case SKI_TC_STATE_COUNT_ZERO: state = "ZER"; break;
					default: state = "???"; break;
				}

				sprintf(str+strlen(str), "[%d, %d, %02d, %02d, %d, %d, %02d, %d, %d, %d, %s] ",
					tc->id,
					tc->exists,
					tc->priority,
					tc->priority_adjust,
					tc->cpu_no,
					tc->is_int,
					tc->int_no,
					tc->reentry_no,
					tc->count,
					ski_cpu_can_start_interrupts(find_cpu(tc->cpu_no)),
					state);
	
			}
		}
		sprintf(str+strlen(str),"\n");
		SKI_TRACE_ACTIVE(str);	
	}
}

static void ski_tc_initialize(thread_context * tc, int id, int priority, int cpu_no, int int_no, int reentry_no){
	int is_int = (int_no >= 0)? 1 : 0;

	tc->priority = priority;
	tc->priority_adjust = 0;
	tc->exists = is_int ? 0 : 1;
	tc->id = id;

	tc->cpu_no = cpu_no;
	tc->is_int = is_int;
	tc->int_no = int_no;
	tc->reentry_no = reentry_no;
	tc->count = 1;

	tc->state = is_int? SKI_TC_STATE_NA : SKI_TC_STATE_RUNNABLE;
}

void ski_tc_dump_all(CPUState *env)
{
	int i;

	SKI_TRACE("ski_tc_dump_all:\n");
	for(i=0;i<ski_threads_no;i++){
		thread_context *tc = &ski_threads[i];
		ski_tc_dump(env,tc);	
	}
	
}

void ski_tc_dump(CPUState *env, thread_context* tc)
{
	char *state;
	switch(tc->state){
		case SKI_TC_STATE_RUNNABLE: state = "RUNNABLE"; break;
		case SKI_TC_STATE_BLOCKED: state = "BLOCKED"; break;
		case SKI_TC_STATE_IS_WAITING: state = "IS_WAITING"; break;
		case SKI_TC_STATE_MAYBE_WAITING: state = "MAYBE_WAITING"; break;
		case SKI_TC_STATE_NA: state = "NA"; break;
		case SKI_TC_STATE_NEEDS_TRIGGER: state = "NEEDS_TRIGGER"; break;
		case SKI_TC_STATE_COUNT_ZERO: state = "COUNT_ZERO"; break;
		default: state = "???"; break;
	}

	SKI_TRACE("ski_tc_dump: id=%d, exists=%d priority=%02d adj=%02d calc=%02d CPU=%d is_int=%d int_no=%02d reentry_no=%d count=%d can_start_interrupts=%d state=%s\n",
		tc->id,
		tc->exists,
		tc->priority,
		tc->priority_adjust,
		ski_threads_calculate_priority(env, tc),
		tc->cpu_no,
		tc->is_int,
		tc->int_no,
		tc->reentry_no,
		tc->count,
		ski_cpu_can_start_interrupts(find_cpu(tc->cpu_no)),
		state);
}

// -------------------------------------------------------------------------------------------------

static inline void ski_set_bit(uint32_t *tab, int index)
{
    int i, mask;
    i = index >> 5;
    mask = 1 << (index & 0x1f);
    tab[i] |= mask;
}


void ski_initialize_interrupts(CPUState *env){
	SKI_TRACE("ski_initialize_interrupts\n");
	memset(env->ski_cpu.ski_ignore_interrupts,0,32);

	// Disable all the timers
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 239);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 48);

	// Disable all IRQ for now
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 48);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 49);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 50);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 51);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 52);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 53);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 54);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 55);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 56);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 57);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 58);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 59);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 60);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 61);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 62);
	ski_set_bit(env->ski_cpu.ski_ignore_interrupts, 63);

	memset(env->ski_cpu.interrupts_stack,0,sizeof(env->ski_cpu.interrupts_stack));
	env->ski_cpu.interrupts_stack_no = 0;

	// Check which are the interrupts already set, and enable them in the threads_context
	int i;
	for(i=0;i<255;i++){
		int res = ski_apic_check_interrupt_set(i, env->apic_state);
		if(res){
			ski_handle_new_irq_set(env, i, -1);
		}
	}

}

// -------------------------------------------------------------------------------------------------

/* main execution loop */

volatile sig_atomic_t exit_request;

int cpu_exec(CPUState *env)
{
    int ret, interrupt_request;
    TranslationBlock *tb;
    uint8_t *tc_ptr;
    unsigned long next_tb;

    if (env->halted) {
        if (!cpu_has_work(env)) {
            return EXCP_HALTED;
        }
		/* XXX: For now disabled this...need to check if it works
		if (env->ski_active  &&  !(ski_threads_current->is_int)){
			// XXX: Not sure if this works....
			// Ideally we should check which threads can do work within our own structures, when chosing the ski_threads_current
			return EXCP_HALTED;
		}*/

        env->halted = 0;
    }

    cpu_single_env = env;

    if (unlikely(exit_request)) {
        env->exit_request = 1;
    }

	SKI_TRACE_ACTIVE("Starting execution cycle\n");

#if defined(TARGET_I386)
    /* put eflags in CPU temporary format */
    CC_SRC = env->eflags & (CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
    DF = 1 - (2 * ((env->eflags >> 10) & 1));
    CC_OP = CC_OP_EFLAGS;
    env->eflags &= ~(DF_MASK | CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
#elif defined(TARGET_SPARC)
#elif defined(TARGET_M68K)
    env->cc_op = CC_OP_FLAGS;
    env->cc_dest = env->sr & 0xf;
    env->cc_x = (env->sr >> 4) & 1;
#elif defined(TARGET_ALPHA)
#elif defined(TARGET_ARM)
#elif defined(TARGET_UNICORE32)
#elif defined(TARGET_PPC)
    env->reserve_addr = -1;
#elif defined(TARGET_LM32)
#elif defined(TARGET_MICROBLAZE)
#elif defined(TARGET_MIPS)
#elif defined(TARGET_SH4)
#elif defined(TARGET_CRIS)
#elif defined(TARGET_S390X)
#elif defined(TARGET_XTENSA)
    /* XXXXX */
#else
#error unsupported target CPU
#endif
    env->exception_index = -1;

	ski_exec_instruction_counter_per_cycle = 0;

    /* prepare setjmp context for exception handling */
    for(;;) {
        if (setjmp(env->jmp_env) == 0) {
            /* if an exception is pending, we execute it here */
			// PF:
			if(ski_snapshot_restoring){
			//if(env->ski_active && ski_snapshot_restoring){
				if(env->ski_cpu.last_enter_cpu){
					ski_snapshot_restoring = 0;
					if (ski_init_options_gdbstub){
						//XXX: Should find a better way...
						int sleeping_period = 10;
						SKI_TRACE("Sleeping for %d seconds to wait for gdb\n", sleeping_period);
						sleep(sleeping_period);
						SKI_TRACE(" -> Finished waiting for gdb\n");
					}
					ski_start_test(env);
				}else{
					break;
				}
			}
			

			if(env->ski_active && ski_threads_need_reevaluate){
				SKI_TRACE_ACTIVE("Reevaluating the current thread\n");
				break;
			}

            if (env->exception_index >= 0) {
//PF:				printf("[CPU %d] env->exception_index = %d [CR3: %x]\n", env->cpu_index, env->exception_index, env->cr[3]);
                if (env->exception_index >= EXCP_INTERRUPT) {
                    /* exit request from the cpu execution loop */
                    ret = env->exception_index;
                    if (ret == EXCP_DEBUG) {
                        cpu_handle_debug_exception(env);
                    }
                    break;
                } else {
#if defined(CONFIG_USER_ONLY)
                    /* if user mode only, we simulate a fake exception
                       which will be handled outside the cpu execution
                       loop */
#if defined(TARGET_I386)
                    do_interrupt(env);
#endif
                    ret = env->exception_index;
                    break;
#else

///PF: Start
   #if defined(TARGET_I386)

					if(env->exception_index==HYPERCALL_INT && env->regs[R_EAX]==HYPERCALL_EAX_MAGIC){
						ski_process_hypercall(env);
					}else{
   #endif 
//PF: End
					SKI_TRACE("Handling exception: %d\n", env->exception_index);
                    do_interrupt(env);
                    env->exception_index = -1;
///PF: Start
   #if defined(TARGET_I386)
   					}
   #endif 
//PF: End
#endif
                }
            }

            next_tb = 0; /* force lookup of first TB */
            for(;;) {
				if(env->ski_active && ski_threads_need_reevaluate){
					SKI_TRACE_ACTIVE("Reevaluating the current thread\n");
					cpu_loop_exit(env);
				}
                interrupt_request = env->interrupt_request;
//PF:
				//SKI_TRACE_ACTIVE("Interrupt analysis cycle\n");
//				if(interrupt_request && env->ski_active){
//					SKI_TRACE("Interrupt: pending interrupt_request = %d\n", interrupt_request);
//				}

//ORIGINAL:	if (unlikely(interrupt_request)) {
//                if (unlikely(interrupt_request && !env->ski_active)) {
//                if (unlikely(interrupt_request && (!env->ski_active || interrupt_request==2))) {
//                if (unlikely(interrupt_request && (!env->ski_active || (ski_apic_process_incoming_interrupts(env) && ski_apic_irq_pending(env->apic_state)>0) || (interrupt_request & ~CPU_INTERRUPT_NMI)!=0))) {
                if (unlikely(interrupt_request && (!env->ski_active || (ski_threads_current->is_int && ski_threads_current->state == SKI_TC_STATE_NEEDS_TRIGGER)))) {
//PF:					printf("[CPU %d] interrupt_request = %d\n",env->cpu_index, interrupt_request);
					SKI_TRACE_ACTIVE("Checking for interrupts (interrupt_request = %d, exception_index = %d, ski_threads_current->id = %d, ski_threads_current->state = %d)\n", 
										interrupt_request, env->exception_index, ski_threads_current->id, ski_threads_current->state);
                    if (unlikely(env->singlestep_enabled & SSTEP_NOIRQ)) {
                        /* Mask out external interrupts for this step. */
                        interrupt_request &= ~CPU_INTERRUPT_SSTEP_MASK;
                    }
                    if (interrupt_request & CPU_INTERRUPT_DEBUG) {
                        env->interrupt_request &= ~CPU_INTERRUPT_DEBUG;
                        env->exception_index = EXCP_DEBUG;
                        cpu_loop_exit(env);
                    }
#if defined(TARGET_ARM) || defined(TARGET_SPARC) || defined(TARGET_MIPS) || \
    defined(TARGET_PPC) || defined(TARGET_ALPHA) || defined(TARGET_CRIS) || \
    defined(TARGET_MICROBLAZE) || defined(TARGET_LM32) || defined(TARGET_UNICORE32)
                    if (interrupt_request & CPU_INTERRUPT_HALT) {
                        env->interrupt_request &= ~CPU_INTERRUPT_HALT;
                        env->halted = 1;
                        env->exception_index = EXCP_HLT;
                        cpu_loop_exit(env);
                    }
#endif
#if defined(TARGET_I386)
                    if (interrupt_request & CPU_INTERRUPT_INIT) {
							SKI_TRACE_INTERRUPTS("Interrupt: CPU_INTERRUPT_INIT\n");
                            svm_check_intercept(env, SVM_EXIT_INIT);
                            do_cpu_init(env);
                            env->exception_index = EXCP_HALTED;
                            cpu_loop_exit(env);
                    } else if (interrupt_request & CPU_INTERRUPT_SIPI) {
							SKI_TRACE_INTERRUPTS("Interrupt: CPU_INTERRUPT_SIPI\n");
                            do_cpu_sipi(env);
                    } else if (env->hflags2 & HF2_GIF_MASK) {
						SKI_TRACE_INTERRUPTS("Interrupt: HF2_GIF_MASK (hflags = %x, hflags2 = %x, eflags = %x)\n", (unsigned int)env->hflags, (unsigned int)env->hflags2, (unsigned int)env->eflags);
                        if ((interrupt_request & CPU_INTERRUPT_SMI) &&
                            !(env->hflags & HF_SMM_MASK)) {
							SKI_TRACE_INTERRUPTS("Interrupt: CPU_INTERRUPT_SMI\n");
                            svm_check_intercept(env, SVM_EXIT_SMI);
                            env->interrupt_request &= ~CPU_INTERRUPT_SMI;
                            do_smm_enter(env);
                            next_tb = 0;
                        } else if ((interrupt_request & CPU_INTERRUPT_NMI) &&
                                   !(env->hflags2 & HF2_NMI_MASK)) {
							SKI_TRACE_INTERRUPTS("Interrupt: CPU_INTERRUPT_NMI\n");
                            env->interrupt_request &= ~CPU_INTERRUPT_NMI;
                            env->hflags2 |= HF2_NMI_MASK;
                            do_interrupt_x86_hardirq(env, EXCP02_NMI, 1);
                            next_tb = 0;
						} else if (interrupt_request & CPU_INTERRUPT_MCE) {
							SKI_TRACE_INTERRUPTS("Interrupt: CPU_INTERRUPT_MCE\n");
                            env->interrupt_request &= ~CPU_INTERRUPT_MCE;
                            do_interrupt_x86_hardirq(env, EXCP12_MCHK, 0);
                            next_tb = 0;
                        } else if ((interrupt_request & CPU_INTERRUPT_HARD) &&
                                   (((env->hflags2 & HF2_VINTR_MASK) && 
                                     (env->hflags2 & HF2_HIF_MASK)) ||
                                    (!(env->hflags2 & HF2_VINTR_MASK) && 
                                     (env->eflags & IF_MASK && 
                                      !(env->hflags & HF_INHIBIT_IRQ_MASK))))
									  // Either ski is not active OR ski is in a state capable of accepting interrupts and there are high priority interrupt threads waiting
///*PF:*/								  &&((!env->ski_active) || (ski_apic_process_incoming_interrupts(env) && ski_apic_irq_pending(env->apic_state)>0))	  
/*PF:*/								  &&((!env->ski_active) || (ski_apic_process_incoming_interrupts(env))) 
									  ) {

							assert((!env->ski_active) || ski_apic_irq_pending(env->apic_state)>0);
                            int intno;
							SKI_TRACE_INTERRUPTS("Interrupt: CPU_INTERRUPT_HARD\n");
                            svm_check_intercept(env, SVM_EXIT_INTR);
                            intno = cpu_get_pic_interrupt(env);
							SKI_ASSERT(intno>=0);
                            env->interrupt_request &= ~(CPU_INTERRUPT_HARD | CPU_INTERRUPT_VIRQ);  // PF: Drop this line because it interfered with the checks made inside the ski_apic functions
							// PF:
							//ski_handle_interrupt_begin(env,intno);
//PF:							printf("[CPU %d] Servicing hardware INT=0x%02x\n", env->cpu_index, intno);
                            SKI_TRACE_INTERRUPTS("Servicing hardware INT=0x%02x\n", intno);
                            qemu_log_mask(CPU_LOG_TB_IN_ASM, "Servicing hardware INT=0x%02x\n", intno);
                            do_interrupt_x86_hardirq(env, intno, 1);
                            /* ensure that no TB jump will be modified as
                               the program flow was changed */
                            next_tb = 0;
#if !defined(CONFIG_USER_ONLY)
                        } else if ((interrupt_request & CPU_INTERRUPT_VIRQ) &&
                                   (env->eflags & IF_MASK) && 
                                   !(env->hflags & HF_INHIBIT_IRQ_MASK)) {
                            int intno;
                            /* FIXME: this should respect TPR */
							SKI_TRACE_INTERRUPTS("Interrupt: CPU_INTERRUPT_VIRQ\n");
                            svm_check_intercept(env, SVM_EXIT_VINTR);
                            intno = ldl_phys(env->vm_vmcb + offsetof(struct vmcb, control.int_vector));
                            SKI_TRACE_INTERRUPTS("Servicing virtual hardware INT=0x%02x\n", intno);
                            qemu_log_mask(CPU_LOG_TB_IN_ASM, "Servicing virtual hardware INT=0x%02x\n", intno);
                            do_interrupt_x86_hardirq(env, intno, 1);
                            env->interrupt_request &= ~CPU_INTERRUPT_VIRQ;
                            next_tb = 0;
#endif
                        }
						SKI_TRACE_INTERRUPTS("Interrupt: out\n");
                    }
#elif defined(TARGET_PPC)
#if 0
                    if ((interrupt_request & CPU_INTERRUPT_RESET)) {
                        cpu_reset(env);
                    }
#endif
                    if (interrupt_request & CPU_INTERRUPT_HARD) {
                        ppc_hw_interrupt(env);
                        if (env->pending_interrupts == 0)
                            env->interrupt_request &= ~CPU_INTERRUPT_HARD;
                        next_tb = 0;
                    }
#elif defined(TARGET_LM32)
                    if ((interrupt_request & CPU_INTERRUPT_HARD)
                        && (env->ie & IE_IE)) {
                        env->exception_index = EXCP_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_MICROBLAZE)
                    if ((interrupt_request & CPU_INTERRUPT_HARD)
                        && (env->sregs[SR_MSR] & MSR_IE)
                        && !(env->sregs[SR_MSR] & (MSR_EIP | MSR_BIP))
                        && !(env->iflags & (D_FLAG | IMM_FLAG))) {
                        env->exception_index = EXCP_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_MIPS)
                    if ((interrupt_request & CPU_INTERRUPT_HARD) &&
                        cpu_mips_hw_interrupts_pending(env)) {
                        /* Raise it */
                        env->exception_index = EXCP_EXT_INTERRUPT;
                        env->error_code = 0;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_SPARC)
                    if (interrupt_request & CPU_INTERRUPT_HARD) {
                        if (cpu_interrupts_enabled(env) &&
                            env->interrupt_index > 0) {
                            int pil = env->interrupt_index & 0xf;
                            int type = env->interrupt_index & 0xf0;

                            if (((type == TT_EXTINT) &&
                                  cpu_pil_allowed(env, pil)) ||
                                  type != TT_EXTINT) {
                                env->exception_index = env->interrupt_index;
                                do_interrupt(env);
                                next_tb = 0;
                            }
                        }
					}
#elif defined(TARGET_ARM)
                    if (interrupt_request & CPU_INTERRUPT_FIQ
                        && !(env->uncached_cpsr & CPSR_F)) {
                        env->exception_index = EXCP_FIQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
                    /* ARMv7-M interrupt return works by loading a magic value
                       into the PC.  On real hardware the load causes the
                       return to occur.  The qemu implementation performs the
                       jump normally, then does the exception return when the
                       CPU tries to execute code at the magic address.
                       This will cause the magic PC value to be pushed to
                       the stack if an interrupt occurred at the wrong time.
                       We avoid this by disabling interrupts when
                       pc contains a magic address.  */
                    if (interrupt_request & CPU_INTERRUPT_HARD
                        && ((IS_M(env) && env->regs[15] < 0xfffffff0)
                            || !(env->uncached_cpsr & CPSR_I))) {
                        env->exception_index = EXCP_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_UNICORE32)
                    if (interrupt_request & CPU_INTERRUPT_HARD
                        && !(env->uncached_asr & ASR_I)) {
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_SH4)
                    if (interrupt_request & CPU_INTERRUPT_HARD) {
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_ALPHA)
                    {
                        int idx = -1;
                        /* ??? This hard-codes the OSF/1 interrupt levels.  */
				        switch (env->pal_mode ? 7 : env->ps & PS_INT_MASK) {
                        case 0 ... 3:
                            if (interrupt_request & CPU_INTERRUPT_HARD) {
                                idx = EXCP_DEV_INTERRUPT;
                            }
                            /* FALLTHRU */
                        case 4:
                            if (interrupt_request & CPU_INTERRUPT_TIMER) {
                                idx = EXCP_CLK_INTERRUPT;
                            }
                            /* FALLTHRU */
                        case 5:
                            if (interrupt_request & CPU_INTERRUPT_SMP) {
                                idx = EXCP_SMP_INTERRUPT;
                            }
                            /* FALLTHRU */
                        case 6:
                            if (interrupt_request & CPU_INTERRUPT_MCHK) {
                                idx = EXCP_MCHK;
                            }
                        }
                        if (idx >= 0) {
                            env->exception_index = idx;
                            env->error_code = 0;
                            do_interrupt(env);
                            next_tb = 0;
                        }
                    }
#elif defined(TARGET_CRIS)
                    if (interrupt_request & CPU_INTERRUPT_HARD
                        && (env->pregs[PR_CCS] & I_FLAG)
                        && !env->locked_irq) {
                        env->exception_index = EXCP_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
                    if (interrupt_request & CPU_INTERRUPT_NMI
                        && (env->pregs[PR_CCS] & M_FLAG)) {
                        env->exception_index = EXCP_NMI;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_M68K)
                    if (interrupt_request & CPU_INTERRUPT_HARD
                        && ((env->sr & SR_I) >> SR_I_SHIFT)
                            < env->pending_level) {
                        /* Real hardware gets the interrupt vector via an
                           IACK cycle at this point.  Current emulated
                           hardware doesn't rely on this, so we
                           provide/save the vector when the interrupt is
                           first signalled.  */
                        env->exception_index = env->pending_vector;
                        do_interrupt_m68k_hardirq(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_S390X) && !defined(CONFIG_USER_ONLY)
                    if ((interrupt_request & CPU_INTERRUPT_HARD) &&
                        (env->psw.mask & PSW_MASK_EXT)) {
                        do_interrupt(env);
                        next_tb = 0;
                    }
#elif defined(TARGET_XTENSA)
                    if (interrupt_request & CPU_INTERRUPT_HARD) {
                        env->exception_index = EXC_IRQ;
                        do_interrupt(env);
                        next_tb = 0;
                    }
#endif
                   /* Don't use the cached interrupt_request value,
                      do_interrupt may have updated the EXITTB flag. */
                    if (env->interrupt_request & CPU_INTERRUPT_EXITTB) {
                        env->interrupt_request &= ~CPU_INTERRUPT_EXITTB;
                        /* ensure that no TB jump will be modified as
                           the program flow was changed */
                        next_tb = 0;
                    }
                }
                if (unlikely(env->exit_request)) {
                    env->exit_request = 0;
                    env->exception_index = EXCP_INTERRUPT;
                    cpu_loop_exit(env);
                }
#if defined(DEBUG_DISAS) || defined(CONFIG_DEBUG_EXEC)
                if (qemu_loglevel_mask(CPU_LOG_TB_CPU)) {
                    /* restore flags in standard format */
#if defined(TARGET_I386)
                    env->eflags = env->eflags | cpu_cc_compute_all(env, CC_OP)
                        | (DF & DF_MASK);
                    log_cpu_state(env, X86_DUMP_CCOP);
                    env->eflags &= ~(DF_MASK | CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
#elif defined(TARGET_M68K)
                    cpu_m68k_flush_flags(env, env->cc_op);
                    env->cc_op = CC_OP_FLAGS;
                    env->sr = (env->sr & 0xffe0)
                              | env->cc_dest | (env->cc_x << 4);
                    log_cpu_state(env, 0);
#else
                    log_cpu_state(env, 0);
#endif
                }
#endif /* DEBUG_DISAS || CONFIG_DEBUG_EXEC */
				if(env->ski_active){
					ski_handle_cpu_begin(env);
				}

                spin_lock(&tb_lock);
                tb = tb_find_fast(env);
                /* Note: we do it here to avoid a gcc bug on Mac OS X when
                   doing it in tb_find_slow */
                if (tb_invalidated_flag) {
                    /* as some TB could have been invalidated because
                       of memory exceptions while generating the code, we
                       must recompute the hash index here */
                    next_tb = 0;
                    tb_invalidated_flag = 0;
                }
#ifdef CONFIG_DEBUG_EXEC
                qemu_log_mask(CPU_LOG_EXEC, "Trace 0x%08lx [" TARGET_FMT_lx "] %s\n",
                             (long)tb->tc_ptr, tb->pc,
                             lookup_symbol(tb->pc));
#endif
                /* see if we can patch the calling TB. When the TB
                   spans two pages, we cannot safely do a direct
                   jump. */
                if (next_tb != 0 && tb->page_addr[1] == -1) {
                    tb_add_jump((TranslationBlock *)(next_tb & ~3), next_tb & 3, tb);
                }
                spin_unlock(&tb_lock);

                /* cpu_interrupt might be called while translating the
                   TB, but before it is linked into a potentially
                   infinite loop and becomes env->current_tb. Avoid
                   starting execution if there is a pending interrupt. */
                env->current_tb = tb;
                barrier();
                if (likely(!env->exit_request)) {
                    tc_ptr = tb->tc_ptr;
                /* execute the generated code */
                    next_tb = tcg_qemu_tb_exec(env, tc_ptr);
                    if ((next_tb & 3) == 2) {
                        /* Instruction counter expired.  */
                        int insns_left;
                        tb = (TranslationBlock *)(long)(next_tb & ~3);
                        /* Restore PC.  */
                        cpu_pc_from_tb(env, tb);
                        insns_left = env->icount_decr.u32;
                        if (env->icount_extra && insns_left >= 0) {
                            /* Refill decrementer and continue execution.  */
                            env->icount_extra += insns_left;
                            if (env->icount_extra > 0xffff) {
                                insns_left = 0xffff;
                            } else {
                                insns_left = env->icount_extra;
                            }
                            env->icount_extra -= insns_left;
                            env->icount_decr.u16.low = insns_left;
                        } else {
                            if (insns_left > 0) {
                                /* Execute remaining instructions.  */
                                cpu_exec_nocache(env, insns_left, tb);
                            }
                            env->exception_index = EXCP_INTERRUPT;
                            next_tb = 0;
                            cpu_loop_exit(env);
                        }
                    }
                }
                env->current_tb = NULL;
                /* reset soft MMU for next block (it can currently
                   only be set by a memory fault) */
            } /* for(;;) */
        } else {
            /* Reload env after longjmp - the compiler may have smashed all
             * local variables as longjmp is marked 'noreturn'. */
            env = cpu_single_env;
        }
    } /* for(;;) */

	if(unlikely(env->ski_active)){
		SKI_TRACE_ACTIVE("Instructions executed in this cycle: %d (total: %d)\n" , ski_exec_instruction_counter_per_cycle, ski_exec_instruction_counter_total);
	}

#if defined(TARGET_I386)
    /* restore flags in standard format */
    env->eflags = env->eflags | cpu_cc_compute_all(env, CC_OP)
        | (DF & DF_MASK);
#elif defined(TARGET_ARM)
    /* XXX: Save/restore host fpu exception state?.  */
#elif defined(TARGET_UNICORE32)
#elif defined(TARGET_SPARC)
#elif defined(TARGET_PPC)
#elif defined(TARGET_LM32)
#elif defined(TARGET_M68K)
    cpu_m68k_flush_flags(env, env->cc_op);
    env->cc_op = CC_OP_FLAGS;
    env->sr = (env->sr & 0xffe0)
              | env->cc_dest | (env->cc_x << 4);
#elif defined(TARGET_MICROBLAZE)
#elif defined(TARGET_MIPS)
#elif defined(TARGET_SH4)
#elif defined(TARGET_ALPHA)
#elif defined(TARGET_CRIS)
#elif defined(TARGET_S390X)
#elif defined(TARGET_XTENSA)
    /* XXXXX */
#else
#error unsupported target CPU
#endif

    /* fail safe : never use cpu_single_env outside cpu_exec() */
    cpu_single_env = NULL;
    return ret;
}
