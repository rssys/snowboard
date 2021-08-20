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


#ifndef SKI_H
#define SKI_H

#include "ski-config.h"

//#define SKI_SAVE_SNAPSHOT 


// PF:

#define SKI_MAX_INPUT_SIZE 100000
#define SKI_HYPERCALL_SYNC_MESSAGE "SYNC"
#define SKI_HYPERCALL_QUIT_MESSAGE "END INFO"
#define SKI_HYPERCALL_NORMAL_SNAPSHOT_MESSAGE "TAKE NORMAL SNAPSHOT"

#define HYPERCALL_INT                   4
#define HYPERCALL_EAX_MAGIC             0x01020304
//#define HYPERCALL_EAX_MAGIC_DEBUG     0x01020305

#define HYPERCALL_IO_MAGIC_START        0x33556677
#define HYPERCALL_IO_MAGIC_END          0x12345678

#define HYPERCALL_IO_TYPE_TEST_ENTER    1
#define HYPERCALL_IO_TYPE_TEST_EXIT     2
#define HYPERCALL_IO_TYPE_DEBUG         3
#define HYPERCALL_IO_TYPE_TRACE_START   4
#define HYPERCALL_IO_TYPE_TRACE_STOP    5
#define HYPERCALL_IO_TYPE_FETCH_DATA    6


//XXX: Must be manually synced with the file used by the guest
typedef struct hypercall_io {
    int magic_start;
    int size;
    int hypercall_type;
    union {
        struct hio_test_enter {
            int gh_nr_instr;
            int gh_nr_cpus;
            int gh_disable_interrupts;
            int hg_res;
        } hio_test_enter;
        struct hio_test_exit {
            int hg_nr_syscalls_self;
            int hg_nr_interrupts_self;
            int hg_nr_syscalls_other;
            int hg_nr_interrupts_other;
            int hg_nr_instr_executed;
            int hg_nr_instr_executed_other;

            int hg_cpu_id;
            int hg_res;
        } hio_test_exit;
        struct hio_debug {
            char gh_msg[128];
        } hio_debug;
        struct hio_fetch_data {
            int seed;
            char progdata[SKI_MAX_INPUT_SIZE];
            int size;
        }hio_fetch_data;
    } p;
    int magic_end;
} hypercall_io;

#define SKI_MAX_RECENT_MEM_W 10
#define SKI_MAX_RECENT_MEM_R 10
#define SKI_MAX_RECENT_IPS 10

typedef struct waiting_context {
    int waiting_for; /* List ? */  /// Waiting_for is not very important
    // TODO: Are this going to be circular buffers?
    int recent_mem_w[SKI_MAX_RECENT_MEM_W];
    int recent_mem_r[SKI_MAX_RECENT_MEM_R];
    int recent_ips[SKI_MAX_RECENT_IPS];
    int waiting_on_loop;

} waiting_context;

// These states reflect the liveness state of the threads if we exclude the effects of our synchronization
#define SKI_TC_STATE_RUNNABLE 1 
#define SKI_TC_STATE_BLOCKED 2
#define SKI_TC_STATE_IS_WAITING 4
#define SKI_TC_STATE_MAYBE_WAITING 8
#define SKI_TC_STATE_NEEDS_TRIGGER 16
#define SKI_TC_STATE_COUNT_ZERO 32
#define SKI_TC_STATE_NA 64

#define SKI_LIVENESS_PRIORITY_HLT 10000
#define SKI_LIVENESS_PRIORITY_PAUSE 5000
#define SKI_LIVENESS_PRIORITY_NOPAUSE_LOOP 2000

// PF: 
typedef struct thread_context {
    int exists;
	int id;
    int priority;
    int priority_adjust;

    int cpu_no;
    int is_int;
    int int_no;
    int reentry_no;
	int count; // counter that goes down to limit the number of instances

	int state;
    waiting_context wc;

    // liveness heuristics
    int last_pause;
    ski_ma wait_rs;
    //int wait_for_interrupt; now on cpu_env
    int wait_for_rs_on_pause;
    int wait_for_rs;


} thread_context;

#define MAX_SKI_THREADS (512)


//#define SKI_EXIT_WATCHDOG 1
//#define SKI_EXIT_NO_RUNNABLE 2
//#define SKI_EXIT_OK 0


// -----------------------------------
// Configuration options

// Also include in the *execution trace* Linux threads that are not part of the test (i.e., ignore the cr3 and the gdt registers)
#define SKI_TRACE_OTHER_LINUX_THREADS


void ski_debug(void);
void ski_threads_dump(CPUState *env);

void ski_first_in_test(CPUState* env);
void ski_start_test(CPUState *env);
void ski_last_in_test(CPUState* env);

int ski_preemption_get_and_clear_lowest(CPUState *env, int min_required);

void ski_sched_setup_initial_priorities(CPUState* env);
CPUState * ski_sched_find_runner(CPUState* env);
void ski_sched_run_next(CPUState *env, char *str);
void ski_sched_invert_priority(CPUState * env);
int ski_sched_adjust_priority_current(CPUState* env);

void ski_deactivate_all_cpu(CPUState* env);
int ski_barrier(CPUState* env, int nr_cpus_barrier, int old_state, int new_state, int blocking_exception, bool call_sched);

void ski_process_hypercall(CPUState *env);

void ski_nontesting_cpus_unblock(CPUState *env);
void ski_nontesting_cpus_block(CPUState *env);

void ski_reset_common(void);

void ski_initialize_interrupts(CPUState *env);

void ski_handle_new_irq_set(CPUState *env, int vector_num, int trigger_mode);
void ski_handle_cpu_begin(CPUState *env);
void ski_handle_interrupt_begin(CPUState *env, int int_no, int is_int, int is_hw);
void ski_handle_interrupt_end(CPUState *env);
void ski_interrupts_dump_stack(CPUState* env);
int ski_interrupts_in_hw_int(CPUState* env, CPUState* penv);

void ski_threads_reset(CPUState *env);
void ski_threads_insert(CPUState *env,  int cpu, int i, int n);

int ski_threads_adjust_priority_current(CPUState* env);
void ski_threads_invert_priority(CPUState * env);
int ski_cpu_can_run(CPUState *env);
int ski_threads_find_next_runnable_int(CPUState *env);

int ski_reset(void);

//void ski_exec_trace_print_comment(char *comment);
//void ski_exec_trace_print_initial_comment(CPUState* env);
//void ski_exec_trace_flush(void);
//void ski_exec_trace_start(CPUState* env);
//void ski_exec_trace_stop(CPUState* env);

void ski_debug_memory(CPUState* env);
void ski_debug_print_config(CPUState* env);

void ski_enable_clock(void);
void ski_disable_clock(void);

void ski_tc_dump_all(CPUState *env);
void ski_tc_dump(CPUState *env, thread_context *tc);

void ski_threads_reevaluate(CPUState *env);

void ski_snapshot_reinitialize(CPUState* env);
void ski_snapshot_reinitialize_cpu(CPUState* env);


extern thread_context ski_threads[MAX_SKI_THREADS];
extern int ski_threads_no;
extern int ski_sched_instructions_current;
extern int ski_sched_instructions_total;
extern int ski_apic_disable_timer;
extern int ski_apic_disable_all;
extern int ski_block_other_cpus;
extern thread_context* ski_threads_current;
extern int ski_snapshot_restoring;

#endif
