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


#ifndef SKI_LIVENESS_H
#define SKI_LIVENESS_H

#include "utlist.h"
#include "uthash.h"

#define SKI_MA_ENTRIES_MAX 20 // Distinct entries

extern bool ski_trace_active;

typedef struct struct_ski_ma_entry{
	int count;
	int eip;
	int mem_address;
	int mem_value;

	struct struct_ski_ma_entry * prev;
	struct struct_ski_ma_entry * next;
	UT_hash_handle hh; 
} ski_ma_entry;


typedef struct struct_ski_ma {
	int is_initialized;
	ski_ma_entry entries[SKI_MA_ENTRIES_MAX];
	int n_entries;
	int max_entries;

	ski_ma_entry * list;
	ski_ma_entry * hash;
} ski_ma;



static inline void ski_liveness_init(ski_ma *ma, int max_entries){
	bzero(ma,sizeof(ski_ma));
	assert(max_entries <= SKI_MA_ENTRIES_MAX);
	ma->max_entries = max_entries;
	ma->is_initialized = 1;
}


static inline int ski_liveness_store(ski_ma *ma, int eip, int mem_address, int mem_value){
	if(!ma->is_initialized)
		return;

	// If mem_address already exists bump counter (but make sure to override the mem_value if necessary and reset the counter)
	ski_ma_entry * mae;
	HASH_FIND_INT(ma->hash, &mem_address, mae);
	if (mae){
		if(mae->mem_value != mem_value){ // Here we're ignoring the eip
			mae->mem_value = mem_value;
			mae->count = 0;
		}
		mae->count++;

		// Move the entry to the end of the list (no need to modify the hash, because it's indexed on the mem_address)
		DL_DELETE(ma->list, mae); 
		DL_APPEND(ma->list, mae);
		return mae->count;
	}


	// Check if there are free entries
	if(unlikely(ma->n_entries < ma->max_entries)){
		mae = &ma->entries[ma->n_entries];
		mae->mem_address = mem_address;
		mae->mem_value = mem_value;
		mae->eip = eip;
		mae->count = 1;
		ma->n_entries++;
		DL_APPEND(ma->list, mae);
		HASH_ADD_INT(ma->hash, mem_address, mae);
		return mae->count;
	}


	// If no free entries, reuse the oldest entry
	mae = ma->list;
	DL_DELETE(ma->list, mae);
	HASH_DEL(ma->hash, mae);
	mae->mem_address = mem_address;
	mae->mem_value = mem_value;
	mae->eip = eip;
	mae->count = 1;
	DL_APPEND(ma->list, mae);
	HASH_ADD_INT(ma->hash, mem_address, mae);
	return mae->count;
}

static inline int ski_liveness_is_present(ski_ma *ma, int mem_address){
	ski_ma_entry * mae;
	HASH_FIND_INT(ma->hash, &mem_address, mae);
	if (mae){
		return 1;
	}
	return 0;
}

static inline void ski_liveness_reset(ski_ma *ma){
	assert(ma->is_initialized);
	ski_liveness_init(ma, ma->max_entries);
}

static inline void ski_liveness_copy(ski_ma *ma_dest, ski_ma *ma_src){
	ski_ma_entry * mae;

	assert(ma_src->is_initialized);

	// memcpy does not work because of the pointers.... memcpy(ma_dest, ma_src, sizeof(ski_ma));
	SKI_INFO_NOCPU("ski_liveness_copy\n");

	ski_liveness_init(ma_dest, ma_src->max_entries);

	DL_FOREACH(ma_src->list, mae) {
		SKI_TRACE_NOCPU("Adding entry to the new RS with mem_address: %x mem_value: %x eip: %x count: %d\n", mae->mem_address, mae->mem_value, mae->eip, mae->count); 
		ski_liveness_store(ma_dest, mae->eip, mae->mem_address, mae->mem_value); //XXX: We're ignoring the count here...probably not necessary... 
	}

}

static inline void ski_liveness_dump(ski_ma *ma){
	ski_ma_entry * mae;

	SKI_TRACE_NOCPU("ski_liveness_dump\n");
	DL_FOREACH(ma->list, mae) SKI_INFO_NOCPU("Mem_address: %x Mem_value: %x eip: %x count: %d\n", mae->mem_address, mae->mem_value, mae->eip, mae->count);
	SKI_TRACE_NOCPU("ski_liveness_dump DONE\n");
}

#endif
