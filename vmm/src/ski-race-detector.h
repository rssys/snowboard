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


#ifndef SKI_RACE_DETECTOR
#define SKI_RACE_DETECTOR

#include <string.h>
#include <assert.h>

#define SKI_RACE_DETECTOR_RACES_MAX 1024
#define SKI_RD_MAX_CPU 4

extern int ski_init_cpu1_preemption;
extern int ski_init_cpu2_preemption;
extern int ski_init_options_preemption_by_eip;

typedef struct struct_ski_rd_memory_access
{
	int cpu;
	int physical_memory_address;
	int ip_address;
	int length; // bits or bytes?
	int is_read;
	int instruction_count;
} ski_rd_memory_access;

typedef struct struct_ski_rd_race
{
	ski_rd_memory_access m1;
	ski_rd_memory_access m2;
} ski_rd_race;

typedef struct struct_ski_race_detector
{
	ski_rd_memory_access last_access_per_cpu[SKI_RD_MAX_CPU];	

	ski_rd_race races[SKI_RACE_DETECTOR_RACES_MAX];
	int races_n;

	int total_races;
} ski_race_detector;


void ski_race_detector_init(ski_race_detector *rd);
void ski_race_detector_print(ski_race_detector *rd, char* trace_filename, int seed, int input1, int input2, FILE *fp_races);

static inline void ski_race_detector_save_race(ski_race_detector *rd, ski_rd_memory_access *m1, ski_rd_memory_access *m2){
    rd->total_races++;

    if(rd->races_n < SKI_RACE_DETECTOR_RACES_MAX){
        ski_rd_race *race = &rd->races[rd->races_n];
        memcpy(&race->m1, m1, sizeof(ski_rd_memory_access));
        memcpy(&race->m2, m2, sizeof(ski_rd_memory_access));
        rd->races_n++;
    }
}


static inline int ski_race_detector_is_race(ski_rd_memory_access *m1, ski_rd_memory_access *m2){
	assert((m1->cpu) != (m2->cpu));
	
	if(m1->is_read && m2->is_read){
		return 0;
	}
    
	if((m1->length==0) || (m2->length == 0)){
		return 0;
	}

	if((m1->physical_memory_address <= m2->physical_memory_address) && ((m1->physical_memory_address + m1->length) > m2->physical_memory_address)){
		return 1;
	}

	if((m2->physical_memory_address <= m1->physical_memory_address) && ((m2->physical_memory_address + m2->length) > m1->physical_memory_address)){
		return 1;
	}

	return 0;
}

static inline void ski_race_detector_new_access(ski_race_detector *rd, int cpu, int physical_memory_address, int ip_address, int length, int is_read, int instruction_count){
	int i;

	assert((cpu>=0) && (cpu<SKI_RD_MAX_CPU));
	
	ski_rd_memory_access *ma = &rd->last_access_per_cpu[cpu];
	ma->physical_memory_address = physical_memory_address;
	ma->ip_address = ip_address;
	ma->length = length;
	ma->is_read = is_read;
	ma->instruction_count = instruction_count;
	int is_preempt=0;
	for(i=0;i<SKI_RD_MAX_CPU;i++){
		if(i!=cpu){
			ski_rd_memory_access *ma2 =  &(rd->last_access_per_cpu[i]);
			int is_race = ski_race_detector_is_race(ma, ma2);
			if(is_race){
				ski_race_detector_save_race(rd, ma, ma2);
			}
		}
	}
}



#endif

