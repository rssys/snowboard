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


#ifndef SKI_BY_EIP_H
#define SKI_BY_EIP_H

typedef struct struct_ski_preeemption_by_eip_entry{
    int eip;
    //int count;
	struct struct_ski_preeemption_by_eip_entry * prev;
	struct struct_ski_preeemption_by_eip_entry * next;
    //UT_hash_handle hh; /* makes this structure hashable */
} ski_preeemption_by_eip_entry;

extern ski_preeemption_by_eip_entry * ski_preeemption_by_eip;


#endif
