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


#ifndef SKI_MEMORY_DETECTOR
#define SKI_MEMORY_DETECTOR

//#define SKI_MEMORY_DETECTOR_ENABLED

#ifdef SKI_MEMORY_DETECTOR_ENABLED


#include <string.h>
#include <assert.h>
#include "uthash.h"

#define SKI_MEMORY_DETECTOR_ENTRIES_MAX (1*1024*1024)
#define SKI_MEMORY_DETECTOR_MA_MAX (10)

#define SKI_MEMORY_DETECTOR_MA_READ 1
#define SKI_MEMORY_DETECTOR_MA_WRITE 2

#define SKI_MEMORY_DETECTOR_HITS_MAX (10*1024)

typedef struct struct_ski_md_ma_entry
{
	int memory_address;
	short type;
	short size_bytes;
} ski_md_ma_entry;

typedef struct struct_ski_md_entry
{
	int eip_address;
	int count;
	ski_md_ma_entry memory_addresses[SKI_MEMORY_DETECTOR_MA_MAX];
	int memory_addresses_n;

	UT_hash_handle hh; /* makes this structure hashable */
} ski_md_entry;

typedef struct struct_ski_md
{
	ski_md_entry instructions[SKI_MEMORY_DETECTOR_ENTRIES_MAX];
	int instructions_n;
	ski_md_entry* instructions_hash;

	int total_instructions;
	int total_distinct_instructions;
} ski_md;


extern int ski_forkall_enabled;

void ski_memory_detector_init(ski_md *md);
void ski_memory_detector_reset(ski_md *md);
void ski_memory_detector_print(ski_md *md, char* trace_filename, int seed, int input1, int input2, FILE *fp_md);


static inline void ski_memory_detector_new_access(ski_md *md, int eip_address, int memory_address, int is_read, int size_bytes){
    int i;
	ski_md_entry* entry = 0;

    HASH_FIND_INT(md->instructions_hash, &eip_address, entry);
      
    if(entry == 0){
        assert(md->instructions_n < SKI_MEMORY_DETECTOR_ENTRIES_MAX);
        entry = &md->instructions[md->instructions_n];
        md->instructions_n++;

        entry->eip_address = eip_address;
		entry->count = 0;
		entry->memory_addresses_n = 0;
        HASH_ADD_INT(md->instructions_hash, eip_address, entry);
    }
	
	if(unlikely(entry->memory_addresses_n <  SKI_MEMORY_DETECTOR_MA_MAX)){
		int found = 0;
		for(i=0;i<entry->memory_addresses_n;i++){
			ski_md_ma_entry *md_entry = &entry->memory_addresses[i];
			if(md_entry->memory_address == memory_address){
				// Update memory access
				md_entry->type |= (is_read ? SKI_MEMORY_DETECTOR_MA_READ : SKI_MEMORY_DETECTOR_MA_WRITE);
				if(md_entry->size_bytes < size_bytes){
					md_entry->size_bytes = (short) size_bytes;
				}
				found = 1;
				break;
			}
		}
	
		if((found == 0) && (entry->memory_addresses_n < SKI_MEMORY_DETECTOR_MA_MAX)){
			// Add memory access
			ski_md_ma_entry *md_entry = &entry->memory_addresses[entry->memory_addresses_n];
			md_entry->memory_address = memory_address;
			md_entry->type |= (is_read ? SKI_MEMORY_DETECTOR_MA_READ : SKI_MEMORY_DETECTOR_MA_WRITE);
			md_entry->size_bytes = (short) size_bytes;
			entry->memory_addresses_n++;
		}
	}

 	if(entry->count == 0){
		md->total_distinct_instructions++;
	}
	entry->count++;
	md->total_instructions++;

    return;
}


#endif // SKI_MEMORY_DETECTOR_ENABLED

#endif

