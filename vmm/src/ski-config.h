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


#ifndef SKI_CONFIG_H
#define SKI_CONFIG_H

// SKI OPTIONS:
// TODO: Add the rest of the options here
#define SKI_SAVE_SNAPSHOT 


#define SKI_MAX_PREEMPTION_POINTS 16

// Used (at least) by the ski_input_number 
#define SKI_CPUS_MAX 16


//PF:
#define SKI_CPU_NA                            0
#define SKI_CPU_BLOCKED_ON_ENTER              1
#define SKI_CPU_RUNNING_TEST                  2
#define SKI_CPU_BLOCKED_ON_EXIT               3

#define SKI_PREEMPTION_MODE_ALL               1
#define SKI_PREEMPTION_MODE_CPU               2
#define SKI_PREEMPTION_MODE_CPU_AND_NONTIMER  3

// TODO: Write down which is specified in MOVs and which are specified in total instructions
// This limit should be sufficient to cover the SKI_SCHED_MAX_MOV_INSTRUCTIONS. XXX: Try to improve these 2 limits.
#define SKI_EXEC_TRACE_MAX_ENTRIES 200000000
//#define SKI_EXEC_TRACE_MAX_ENTRIES 1000000
// After this limit is reached ski is resetted. 
#define SKI_SCHED_MAX_PREEMPTION_INSTRUCTIONS 100000000

#define SKI_SCHED_ADJUST_MODULO 500000
//#define SKI_SCHED_ADJUST_MODULO 50000
//#define SKI_SCHED_ADJUST_MODULO 20000
#define SKI_SCHED_ADJUST_INC 20
//#define SKI_SCHED_ADJUST_INC 20

//Just a temporary method to invert the order of the priorities for the 
// If seed <= SKI_SCHED_SEED_FLIP: CPU1 higest priority and then CPU2 second-highest priority
// else: flip the order of those 2 CPUs
// Obsolete by the priority file: #define SKI_SCHED_SEED_FLIP 100000

// Every x number of cycle iterations print a debug message inside the loop 
#define SKI_DEBUG_LOOP_CYCLE_IN_PROGREESS 50000
#define SKI_DEBUG_MAX_ADJUST 500


#define SKI_IPFILTER_HASH 

#define SKI_MEMORY_INTERCEPT
#define SKI_MEMORY_INTERCEPT_EXECUTION_FILE
#define SKI_MEMORY_INTERCEPT_OWN_FILE

#endif
