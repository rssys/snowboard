/*
 * SKI - Systematic Kernel Interleaving explorer (http://ski.mpi-sws.org)
 *
 * Copyright (c) 2013-2015 Pedro Fonseca
 *
 *
 * This work is licensed under the terms of the GNU GPL, version 3.  See
 * the GPL3 file in SKI's top-level directory.
 *
 */



#include "config.h"
#include "cpu.h"
#include "disas.h"
#include "tcg.h"
#include "qemu-barrier.h"
#include "ski.h"
#include "ski-config.h"
#include "ski-stats.h"
#include "ski-debug.h"
#include <stdio.h>
#include <sys/stat.h>

extern int ski_init_options_quit_hypercall_threshold;
extern int ski_init_options_seed;
extern int ski_init_options_input_number[SKI_CPUS_MAX];
extern int ski_init_options_debug_exit_after_hypercall_enabled;

extern int ski_init_options_cpu_1_mode;
extern int ski_init_options_cpu_2_mode;
extern char ski_init_options_corpus_path[1024];


int ski_quit_hypercall_counter=0; // No need to intialize for each test, because it quits

extern int ski_time_expansion_enable;

void ski_process_hypercall(CPUState *env){
    int guest_value;
    int res;
    hypercall_io hio;

    ski_trace_active=true;

    SKI_TRACE("Hypercall! (env->exception_index = %d)\n", env->exception_index);
    
	env->eip+=2;
	env->exception_index = -1;  /* Ensure that we don't handle the interrupt twice */

    bzero(&hio, sizeof(hypercall_io));
    SKI_TRACE("Reading from guest the result of the hypercall\n");
    res = cpu_memory_rw_debug(env, env->regs[R_ECX] + env->segs[R_DS].base, &hio, sizeof(hio), 0);

    if(hio.magic_start != HYPERCALL_IO_MAGIC_START || hio.magic_end != HYPERCALL_IO_MAGIC_END || hio.size != sizeof(hio)){
        SKI_TRACE("Hypercall_io structure corrupted (0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n",
                hio.magic_start, HYPERCALL_IO_MAGIC_START, hio.magic_end, HYPERCALL_IO_MAGIC_END, hio.size, sizeof(hio));
        SKI_TRACE("Reading from user-land: res: %d, value: %d (ECX: %d, DS.base: %d)\n", res, guest_value, env->regs[R_ECX], env->segs[R_DS].base);
        assert(-1);
    }
    SKI_TRACE("Reading from user-land: res: %d, value: %d (ECX: %d, DS.base: %d)\n", res, guest_value, env->regs[R_ECX], env->segs[R_DS].base);
	SKI_TRACE("ski_process_hypercall: ski_sched_instructions_total = %d ski_sched_instructions_current = %d\n", ski_sched_instructions_total, ski_sched_instructions_current);

    switch(hio.hypercall_type){
        case HYPERCALL_IO_TYPE_TEST_ENTER:
			//ski_time_expansion_enable = 1;  XXX: This should also be enable during snapshot resume init
            env->ski_active = true; // Comes before the TRACE so it gets the active stamp
            ski_first_in_test(env);
            SKI_TRACE("Hypercall: HYPERCALL_IO_TYPE_TEST_ENTER\n");
            env->exception_index = -1;

            env->ski_cpu.priority=-1;
            env->ski_cpu.priority_adjust=0;
            env->ski_cpu.blocked=true;

            env->ski_cpu.nr_max_instr= hio.p.hio_test_enter.gh_nr_instr;
            env->ski_cpu.nr_instr_executed_other = 0;
            env->ski_cpu.nr_instr_executed = 0;
            env->ski_cpu.nr_cpus = hio.p.hio_test_enter.gh_nr_cpus;

            env->ski_cpu.cr3 = env->cr[3];
            env->ski_cpu.gdt = env->gdt.base;
            env->ski_cpu.state = SKI_CPU_BLOCKED_ON_ENTER;

            ski_initialize_interrupts(env);

            env->ski_cpu.nr_syscalls_self = 0;
            env->ski_cpu.nr_interrupts_self = 0;
            env->ski_cpu.nr_syscalls_other = 0;
            env->ski_cpu.nr_interrupts_other = 0;

			env->ski_cpu.last_enter_cpu = 0;

            SKI_TRACE("Writing to guest the result of the hypercall\n");
			if(ski_init_options_input_number[env->cpu_index]){
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
			// XXX: This does not work with the forkall optimization...need to do it agian in the ski_init probably in the cpu_exec during snapshot resume
            res = cpu_memory_rw_debug(env, env->regs[R_ECX] + env->segs[R_DS].base, &hio, sizeof(hio), 1);

			ski_liveness_init(&env->ski_cpu.cpu_rs, SKI_MA_ENTRIES_MAX);

            ski_barrier(env, env->ski_cpu.nr_cpus, SKI_CPU_BLOCKED_ON_ENTER, SKI_CPU_RUNNING_TEST, EXCP_SKI_ENTER_WAITING, false);
            // Only the last returns from ski_barrier
			
			env->ski_cpu.last_enter_cpu = 1;

			// TODO: Perhaps make this conditional
			// Make snapshot
#ifdef SKI_SAVE_SNAPSHOT
			SKI_TRACE("Starting snapshot...\n");
		    ski_do_savevm();
			SKI_TRACE("Snapshot finished\n");
#else
			SKI_TRACE("Snapshot disabled\n");
#endif

			ski_start_test(env);
            // Possibly does not return
            break;
        case HYPERCALL_IO_TYPE_TEST_EXIT:
            SKI_TRACE("Hypercall: HYPERCALL_IO_TYPE_TEST_EXIT\n");
            if(env->ski_active==false){
                SKI_TRACE("[WARNING] Ignoring hypercall exit because SKI not active\n");
                break;
            }

            env->ski_cpu.state = SKI_CPU_BLOCKED_ON_EXIT;

            hio.p.hio_test_exit.hg_nr_syscalls_self = env->ski_cpu.nr_syscalls_self;
            hio.p.hio_test_exit.hg_nr_interrupts_self = env->ski_cpu.nr_interrupts_self;
            hio.p.hio_test_exit.hg_nr_syscalls_other = env->ski_cpu.nr_syscalls_other;
            hio.p.hio_test_exit.hg_nr_interrupts_other = env->ski_cpu.nr_interrupts_other;

            hio.p.hio_test_exit.hg_nr_instr_executed = env->ski_cpu.nr_instr_executed;
            hio.p.hio_test_exit.hg_nr_instr_executed_other = env->ski_cpu.nr_instr_executed_other;

            SKI_TRACE("Writing to guest the result of the hypercall\n");
            res = cpu_memory_rw_debug(env, env->regs[R_ECX] + env->segs[R_DS].base, &hio, sizeof(hio), 1);
				
			char debug_str[128];
		    sprintf(debug_str, "CPU %d reached exit hypercall", env->cpu_index);
		    ski_exec_trace_print_comment(debug_str);

            ski_barrier(env, env->ski_cpu.nr_cpus, SKI_CPU_BLOCKED_ON_EXIT, SKI_CPU_NA, EXCP_SKI_EXIT_WAITING, true);
            // Only the last returns from ski_barrier()

			if(ski_forkall_enabled && ski_init_options_debug_exit_after_hypercall_enabled){
				SKI_ASSERT_MSG(0, SKI_EXIT_AFTER_HYPERCALL, "AFTER_HYPER");
			}

            ski_last_in_test(env);
            ski_deactivate_all_cpu(env);

            // TODO: Clear the active and reset all the structures. Do this perhaps on the exception handler
            break;
        case HYPERCALL_IO_TYPE_DEBUG:
            SKI_TRACE("Hypercall: HYPERCALL_IO_TYPE_DEBUG\n");
            SKI_INFO("=======> MSG: \"%s\"\n", hio.p.hio_debug.gh_msg);
			ski_msg_trace_print(env->cpu_index, hio.p.hio_debug.gh_msg);

			if(strcmp(hio.p.hio_debug.gh_msg, SKI_HYPERCALL_SYNC_MESSAGE)==0){
				SKI_TRACE("Performing sync() on user request\n");
				sync();
				SKI_TRACE("Finished performing the sync() on user request\n");
			}

			//if(ski_init_options_quit && strcmp(hio.p.hio_debug.gh_msg, SKI_HYPERCALL_QUIT_MESSAGE)==0){
			if(ski_init_options_quit_hypercall_threshold && strcmp(hio.p.hio_debug.gh_msg, SKI_HYPERCALL_QUIT_MESSAGE)==0){
				ski_quit_hypercall_counter++;
                printf("Captured a quit request\n");
				if(ski_quit_hypercall_counter==ski_init_options_quit_hypercall_threshold){
					SKI_INFO("Exiting (on hypercall request)!\n");
					printf("[SKI] prepare for quit request!\n");
					
					/*
					// SLOW SLOW SLOW SLOW SLOW SLOW SLOW SLOW SLOW
					SKI_INFO("Marking the end with multiple sync calls![SLOW SLOW SLOW]\n");
					int i;
					for(i=0;i<5;i++){
						sync();
					}
					*/
					/*
					printf("Sleeping for 10 seconds!!!!\n");
					sleep(10)
					*/
					SKI_STATS_FINISH_SUCCESS();

					ski_stats_finish(ski_sched_instructions_total);
					
					printf("[SKI] Exiting on hypercall request!\n");

					SKI_ASSERT_MSG(0, SKI_EXIT_OK, "HYPER_REQUEST");

					exit(0);	
				}else{
					SKI_INFO("Waiting for %d more quit hypercall requests before quiting\n", ski_init_options_quit_hypercall_threshold - ski_quit_hypercall_counter);
				}
			}


			if(strcmp(hio.p.hio_debug.gh_msg, SKI_HYPERCALL_NORMAL_SNAPSHOT_MESSAGE)==0){
				SKI_INFO("Taking normal snapshot (on user request)!\n");
	            SKI_TRACE("Starting snapshoot...\n");
				ski_do_savevm();
				SKI_TRACE("Flushing disk\n");
				fflush(0);
				sync();
				SKI_TRACE("Snapshoot finished\n");
			}

            break;

        case HYPERCALL_IO_TYPE_TRACE_START:
            SKI_TRACE("Hypercall: HYPERCALL_IO_TYPE_TRACE_START\n");
            //TODO:
            ski_exec_trace_start(env);
            break;
        case HYPERCALL_IO_TYPE_TRACE_STOP:
            SKI_TRACE("Hypercall: HYPERCALL_IO_TYPE_TRACE_END\n");
            //TODO:
            ski_exec_trace_stop(env);
            break;
        case HYPERCALL_IO_TYPE_FETCH_DATA:
            ;
            FILE * data_fd=NULL;
            char filename[1024];
            sprintf(filename, "%s/%d.data", ski_init_options_corpus_path, hio.p.hio_fetch_data.seed);
            //printf("SKI DEBUG DEBUG ======== %s ==============\n", filename);
            struct stat statbuf;
            stat(filename, &statbuf);
            if (statbuf.st_size > SKI_MAX_INPUT_SIZE){
                // Simply skip this input if it is too large
                hio.p.hio_fetch_data.size = 0;
                break;
            }
            data_fd = fopen(filename, "r");
            if (data_fd == NULL){
                hio.p.hio_fetch_data.size = 0;
                break;
            }
            //printf("SKI open file %d\n", data_fd);
            int index=0;
            int size = fread(hio.p.hio_fetch_data.progdata, 1, SKI_MAX_INPUT_SIZE, data_fd);
            fclose(data_fd);
            //printf("SKI_DEBUG_SIZE=%d\n", size);
            hio.p.hio_fetch_data.size = size;
            //for (index=0; index < size; index++)
            //     printf("%u ", (unsigned char)hio.p.hio_fetch_data.progdata[index]);
            //printf("SKI_debug hio size is %d\n", sizeof(hio));
            res = cpu_memory_rw_debug(env, env->regs[R_ECX] + env->segs[R_DS].base, &hio, sizeof(hio), 1);

            break;
        default:
            SKI_TRACE("Unrecognized type (hio.type: %d))\n", hio.hypercall_type);
            break;
            /*TODO: PF initialize the rest of the variables */
    }
}


