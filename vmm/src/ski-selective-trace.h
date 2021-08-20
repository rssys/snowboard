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


#ifndef SKI_SELECTIVE_TRACE_H
#define SKI_SELECTIVE_TRACE_H

// XXX: not fully implemented yet
//#define SKI_SELECTIVE_TRACE_ENABLED

#ifdef SKI_SELECTIVE_TRACE_ENABLED
#include <string.h>
#include <assert.h>
#include "uthash.h"

#define SKI_INSTRUCTION_DETECTOR_ENTRIES_MAX (128*1024)

typedef struct struct_ski_selective_trace_entry
{
	int eip_address;
	int count;

	UT_hash_handle hh; /* makes this structure hashable */
} ski_selective_trace_entry;

typedef struct struct_ski_selective_trace
{
	ski_selective_trace_entry instructions[SKI_INSTRUCTION_DETECTOR_ENTRIES_MAX];
	int instructions_n;
	ski_selective_trace_entry* instructions_hash;

	int total_instructions;
	int total_distinct_instructions;
} ski_selective_trace;


extern char ski_init_options_instructions_detector_filename[1024];
extern int ski_forkall_enabled;

void ski_selective_trace_init(ski_selective_trace *id);
void ski_selective_trace_load(ski_selective_trace *id);
void ski_selective_trace_reset_count(ski_selective_trace *id);

void ski_selective_trace_print(ski_selective_trace *id, char* trace_filename, int seed, int input1, int input2, FILE *fp_id);


static inline void ski_selective_trace_new_access(ski_selective_trace *id, int eip_address){
    ski_selective_trace_entry* entry = 0;

	if(likely((ski_init_options_instructions_detector_filename[0] == 0) || (!ski_forkall_enabled))){
		return;
	}

    HASH_FIND_INT(id->instructions_hash, &eip_address, entry);
       
    if(likely(entry == 0)){
        return 1;
    }  
	if(entry->count == 0){
		id->total_distinct_instructions++;
	}
	entry->count++;
	id->total_instructions++;

    return 0;

}


#endif // SKI_SELECTIVE_TRACE_ENABLED

#endif

