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


#ifndef SKI_DEBUG_H
#define SKI_DEBUG_H

#include "ski-stats.h"

// Avoid having to include the sysemu.h header
extern int ski_init_options_debug_assert_trap_enabled;


#define SKI_NO_OUTPUT // Disables debuging output regarding the VMM/CPU details
#define SKI_DISABLE_FORKALL_OUTPUT // Disable debugging output related to forkall

//#define SKI_DISABLE_TRACE_OUTPUT  // Disables tracing instrumentation (both instruction and memory)


void ski_exec_redirect_to_null(char* filename);

void ski_control_forkall_trace_start(void);
void ski_control_parameters_print(ski_stats *stats);
void ski_control_preemptionlist_print(int eip_address);

void ski_msg_trace_start(void);
void ski_msg_trace_stop(void);
void ski_msg_trace_print(int cpu, char *msg);

void ski_st_trace_start(void);
void ski_st_trace_stop(void);
void ski_st_trace_print(char *msg);

void ski_rd_print(ski_stats *stats);

void ski_heuristics_statistics_start();
void ski_heuristics_statistics_stop();
void ski_heuristics_statistics_print(int heuristic_type, int instruction_address, int cpu_no, int instruction_counter);

typedef struct CPUX86State CPUX86State;
#define CPUState struct CPUX86State


void ski_exec_trace_start(CPUState* env);
void ski_exec_trace_print_comment(char *comment);
void ski_exec_trace_stop(CPUState* env);
void ski_exec_trace_flush(void);
void ski_exec_trace_print_comment(char *comment);
void ski_exec_trace_print_initial_comment(CPUState* env);


#define SKI_EXIT_OK 4
#define SKI_EXIT_WATCHDOG 1
#define SKI_EXIT_NO_RUNNABLE 2

#define SKI_EXIT_MEMFS 3
#define SKI_EXIT_MEMFS_LOWMEM 3
#define SKI_EXIT_MAX_PRIORITY_ADJ 5
#define SKI_EXIT_MAX_PREEMPTION_INST 6
#define SKI_EXIT_ASSERT_INTERRUPT 7
#define SKI_EXIT_AFTER_HYPERCALL 8

#define SKI_EXIT_OTHER -1

extern int ski_forkall_enabled;
void ski_stats_compute_communication_instructions();

// XXX: Call ski_stats_compute_communication_instructions from a better place...
#define SKI_ASSERT_MSG(exp, exit_code, exit_msg)							\
{																			\
	if(!(exp)){																\
		printf("[SKI] [EXIT] %s:%d - Exiting! %s (code %d)\n",			\
				__FILE__, __LINE__, exit_msg, exit_code);					\
		SKI_STATS_ASSERT_LOG(exp, exit_code, exit_msg);			\
		if(ski_forkall_enabled){ ski_stats_compute_communication_instructions();}	\
		exit(exit_code);													\
	}																		\
}

#define SKI_ASSERT_MSG_SIGNAL_SAFE(exp, exit_code, exit_msg)							\
{																			\
	if(!(exp)){																\
		SKI_STATS_ASSERT_LOG(exp, exit_code, exit_msg);			\
		if(ski_forkall_enabled){ ski_stats_compute_communication_instructions();}	\
		_exit(exit_code);													\
	}																		\
}



#ifdef SKI_NO_OUTPUT

#define SKI_TRACE_ACTIVE(...)                                                 
#define SKI_TRACE_ACTIVE_AND_EXEC(...)                                        
#define SKI_TRACE(...)                                                        
#define SKI_INFO(...)                                                         
#define SKI_TRACE_NOCPU(...)                                                   
#define SKI_INFO_NOCPU(...)                                                   

#else

extern FILE * ski_run_trace_file;

//PF:
#define SKI_TRACE_ACTIVE(...)                                                 \
{                                                                               \
    if(unlikely(ski_trace_active && env->ski_active)){                      \
		fprintf(ski_run_trace_file, "[CPU %d] (SKI: %d) ",env->cpu_index, env->ski_active?1:0);  \
		fprintf(ski_run_trace_file, __VA_ARGS__);                                                    \
    }                                                                           \
}

#define SKI_TRACE_ACTIVE_AND_EXEC(...)                                        \
{                                                                               \
    if(unlikely(ski_trace_active && env->ski_active)){                      \
		fprintf(ski_run_trace_file, "[CPU %d] (SKI: %d) ",env->cpu_index, env->ski_active?1:0);  \
		fprintf(ski_run_trace_file, __VA_ARGS__);                                                    \
		if(ski_exec_trace_execution_fd && (ski_exec_trace_nr_entries < SKI_EXEC_TRACE_MAX_ENTRIES)){	\
			fprintf(ski_exec_trace_execution_fd, "### ");						\
			fprintf(ski_exec_trace_execution_fd, __VA_ARGS__);				\
		}																		\
    }                                                                           \
}

#define SKI_TRACE(...)                                                        \
{                                                                               \
   if(unlikely(ski_trace_active)){                                           \
		fprintf(ski_run_trace_file, "[CPU %d] (SKI: %d) ",env->cpu_index, env->ski_active?1:0);  \
		fprintf(ski_run_trace_file, __VA_ARGS__);                                                    \
    }                                                                           \
}

#define SKI_INFO(...)                                                         \
{                                                                               \
	fprintf(ski_run_trace_file, "[CPU %d] (SKI: %d) ",env->cpu_index, env->ski_active?1:0);      \
	fprintf(ski_run_trace_file, __VA_ARGS__);                                                        \
}


#define SKI_TRACE_NOCPU(...)													\
{                                                                               \
   if(unlikely(ski_trace_active)){											\
        fprintf(ski_run_trace_file, "[CPU ?] (SKI: ?) ");											\
        fprintf(ski_run_trace_file, __VA_ARGS__);                                                    \
    }                                                                           \
}

#define SKI_INFO_NOCPU(...)                                                   \
{                                                                               \
    fprintf(ski_run_trace_file, "[CPU ?] (SKI: ?) ");												\
    fprintf(ski_run_trace_file, __VA_ARGS__);                                                        \
}


#endif // SKI_NO_OUTPUT


#define SKI_ASSERT(x)                                 \
{                                                       \
    if(!(x) && ski_init_options_debug_assert_trap_enabled){               \
        __asm__ ( "int $0x3;" );                        \
    }                                                   \
}                                                       \


#endif
