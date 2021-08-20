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


#ifndef SKI_STATS
#define SKI_STATS

// DEBUG UTHASH XXX: Very slow
//#define HASH_DEBUG 1

#include "uthash.h"
#include <sys/time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h> 

#include "ski-config.h"
#include "ski-ipfilter.h"
#include "ski-race-detector.h"
#include "ski-instruction-detector.h"
#include "ski-memory-detector.h"
#include "ski-selective-trace.h"

#define SKI_STATS_MAX_CPU 4
#define SKI_STATS_MAX_INSTRUCTION_ADDRESSES 1*1000*1000
#define SKI_STATS_MAX_DATA_ADDRESSES 1*1000*1000
#define SKI_STATS_MAX_ACCESS_SIZE 9
#define SKI_STATS_MAX_DATA_INSTRUCTION_ADDRESSES 1*1000*1000
#define SKI_STATS_MAX_DATA_VALUE 1000*1000

typedef struct struct_ski_stats_instruction_accesses{
	unsigned int eip_address;
	
	// size of the access does not make sense for instructions

	//long long x_cpu[SKI_STATS_MAX_CPU];
	int cpu_no;

	long long int total_accesses;

	UT_hash_handle hh; /* makes this structure hashable */
} ski_stats_instruction_accesses;


typedef struct struct_ski_stats_value{
	unsigned int value;
	
	UT_hash_handle hh;
} ski_stats_value;

typedef struct struct_ski_stats_instruction{
	unsigned int eip_address;
	
	UT_hash_handle hh; /* makes this structure hashable */
	int is_write;
	//ski_stats_value * data_value_hash;
} ski_stats_instruction;



#define SKI_STATS_SET_W_CPU(data_access, cpu_no) (data_access->rw_cpu_size_flag |= (1 << (cpu_no + 8)))
#define SKI_STATS_SET_R_CPU(data_access, cpu_no) (data_access->rw_cpu_size_flag |= (1 << (cpu_no)))
#define SKI_STATS_SET_SIZE(data_access, size_bytes) (data_access->rw_cpu_size_flag |= (1 << (size_bytes + 16)))

#define SKI_STATS_GET_W_CPU(data_access, cpu_no) (data_access->rw_cpu_size_flag & (1 << (cpu_no + 8)))
#define SKI_STATS_GET_R_CPU(data_access, cpu_no) (data_access->rw_cpu_size_flag & (1 << (cpu_no)))
#define SKI_STATS_GET_SIZE(data_access, size_bytes) (data_access->rw_cpu_size_flag & (1 << (size_bytes + 16)))
 
 

typedef struct struct_ski_stats_data_accesses{
	unsigned int data_address;

	//long long size_bytes[SKI_STATS_MAX_ACCESS_SIZE];

	//long long r_cpu[SKI_STATS_MAX_CPU];
	//long long w_cpu[SKI_STATS_MAX_CPU];
	unsigned int rw_cpu_size_flag;
	long long int total_accesses;

	ski_stats_instruction * data_instructions_hash;

	UT_hash_handle hh; /* makes this structure hashable */
} ski_stats_data_accesses;


typedef struct struct_ski_stats{
	int magic_begin;

	// Used to distribute between processes
	int pid;
	int slot;
	int seed;
	//int round;

	int input_number[SKI_CPUS_MAX];

	ski_stats_instruction_accesses instructions[SKI_STATS_MAX_INSTRUCTION_ADDRESSES];
	long long instructions_n;
	ski_stats_instruction_accesses * instructions_hash;

	ski_stats_data_accesses data[SKI_STATS_MAX_DATA_ADDRESSES];
	long long data_n;
	ski_stats_data_accesses * data_hash;
	ski_stats_instruction data_instructions[SKI_STATS_MAX_DATA_INSTRUCTION_ADDRESSES];
	long long data_instructions_n;
	//ski_stats_value data_values[SKI_STATS_MAX_DATA_VALUE];
	//long long values_n;

	char trace_filename[512];
	char trace_filename_full[512];
	char destination_dir[1024];
	char write_filename[512];
	char read_filename[512];
	char exec_filename[512];
	int preemption_list_size;
	int preemption_points_executed;

	int preemptions[SKI_MAX_PREEMPTION_POINTS];
	int preemptions_len;

	int communication_data;
	int communication_eips;

	long long instructions_executed;
	int ski_cpu1_accessed;
	int ski_cpu2_accessed;
	int ski_cpu1_hint_addr;
	int ski_cpu1_hint_ins;
	int ski_cpu2_hint_addr;
	int ski_cpu2_hint_ins;
	int channel_hit;
	int communicate;
	int ski_write_endpoint;
	int ski_read_endpoint;
	int ski_communication_addr;
	int ski_write_endpoint_value;
	int ski_read_endpoint_value;

	int exit_code;
	char exit_location[128];
	char exit_reason[128];

	struct timeval start;
	struct timeval finish;

	// Intermediate structure that is computed by the child process and then must be read sequentially by the parent (i.e. without using the hashes) 
	ski_stats_instruction* communication_instructions_hash;
	ski_stats_instruction communication_instructions[MAX_IPFILTER_HASH];
	int communication_instructions_n;

	// Race detector data
	ski_race_detector rd;

#ifdef SKI_MEMORY_DETECTOR_ENABLED
	// Memory detector data
	ski_md md;
#endif

	// Instruction detector data
	ski_instruction_detector id;

#ifdef SKI_SELECTIVE_TRACE_ENABLED
	// Selective trace data
	ski_selective_trace st;
#endif

	// XXX: not sure if this is usefull
	char padding[1024*4];

	int magic_end;

} ski_stats;


extern ski_stats* ski_stats_self;


void ski_stats_init_slot(int slot);
void ski_stats_init_all(int n_slots);
void ski_stats_reset_slot(int slot);
void ski_stats_set_self_slot(int slot);
ski_stats *ski_stats_get_self();
ski_stats *ski_stats_get_from_pid(int pid);


void ski_stats_start(void);
void ski_stats_finish();

static inline void ski_stats_add_data_access(unsigned int data_address, unsigned int eip_address, unsigned int size_bytes, int is_write, unsigned int cpu_no);
//static inline void ski_stats_add_data_access(unsigned int data_address, unsigned int eip_address, unsigned int size_bytes, int is_write, unsigned int cpu_no, unsigned int value);
static inline void ski_stats_add_instruction_access(int eip_address, int cpu_no);
void ski_stats_add_trace(char *trace_filename, char* trace_filename_full);
void ski_stats_add_round(int round);
void ski_stats_add_seed(int seed);
void ski_stats_add_preemption_list_size(int size);
void ski_stats_add_write(char* filename);
void ski_stats_add_read(char * filename);
void ski_stats_add_exec(char * filename);
void ski_stats_dump_set();

int ski_stats_input_is_running(ski_stats *stats);
void ski_stats_input_set_preemptions();
void ski_stats_compute_communication();
void ski_stats_dump_all(void);

void ski_stats_dump_all_slots();
void ski_stats_dump_slot(int slot);


static inline void ski_stats_add_data_access_byte(unsigned int data_address, unsigned int eip_address, int is_write, unsigned int cpu_no){
//static inline void ski_stats_add_data_access_byte(unsigned int data_address, unsigned int eip_address, int is_write, unsigned int cpu_no, unsigned int value){
	int size_bytes = 1;
    ski_stats_data_accesses * da;
    HASH_FIND_INT(ski_stats_self->data_hash, &data_address, da);

    if(da == 0){
        // Address not there yet. Have to insert a new entry
        if(!(ski_stats_self->data_n < SKI_STATS_MAX_DATA_ADDRESSES)){
			// XXX: Ignore if there is no space for another memory address!
			static int first_exception=0;
			if(first_exception==0){
				printf("[SKI] [STATS] WARNING: Reached the SKI_STATS_MAX_DATA_ADDRESSES maximum\n");
				first_exception=1;
			}
			return;
		}
		assert(ski_stats_self->data_n < SKI_STATS_MAX_DATA_ADDRESSES);

        da = &ski_stats_self->data[ski_stats_self->data_n];
        ski_stats_self->data_n++;
        da->data_address = data_address;
		da->rw_cpu_size_flag = 0;
		da->data_instructions_hash = 0;
		da->total_accesses = 0;
        HASH_ADD_INT(ski_stats_self->data_hash, data_address, da);
    }

    SKI_STATS_SET_SIZE(da, size_bytes);
	if(is_write){
			SKI_STATS_SET_W_CPU(da, cpu_no);
	}else{
			SKI_STATS_SET_R_CPU(da, cpu_no);
	}
	// Write and read are not differentiated for toal_accesses
    da->total_accesses++;

/* // Keep access size statistics 
    da->size_bytes[size_bytes]++;

    // Keep r&w statistics per CPU
    if(is_write){
        da->w_cpu[cpu_no]++;
    }else{
        da->r_cpu[cpu_no]++;
    }
*/

    // Maintain a list of unique eip addresses resposible for the accesses on this data address
    ski_stats_instruction *di;
    HASH_FIND_INT(da->data_instructions_hash, &eip_address, di);
    if(di == 0){
        // Address not there yet. Have to instert a new intruction entry
        if(!(ski_stats_self->data_instructions_n < SKI_STATS_MAX_DATA_INSTRUCTION_ADDRESSES)){
			// XXX: Ignore if there is no space for another instruction address
			static int first_exception=0;
			if(first_exception==0){
				printf("[SKI] [STATS] WARNING: Reached the SKI_STATS_MAX_DATA_INSTRUCTION_ADDRESSES maximum\n");
				first_exception=1;
			}
			return;
		}

        assert(ski_stats_self->data_instructions_n < SKI_STATS_MAX_DATA_INSTRUCTION_ADDRESSES);
        di = &ski_stats_self->data_instructions[ski_stats_self->data_instructions_n];
        ski_stats_self->data_instructions_n++;
        di->eip_address = eip_address;
		di->is_write = is_write;
		HASH_ADD_INT(da->data_instructions_hash, eip_address, di);
       	//di->data_value_hash=0; 
		//HASH_ADD_INT(da->data_instructions_hash, eip_address, di);
    }
	/*
	// Sishuai: Maintain a list of unique values responsible for the access
	ski_stats_value *dv;
	HASH_FIND_INT(di->data_value_hash, &value, dv);
	if (dv == 0){
		if (!(ski_stats_self->values_n <
					SKI_STATS_MAX_DATA_VALUE)){
			return;		
		}
		dv = &ski_stats_self->data_values[ski_stats_self->values_n];
		ski_stats_self->values_n++;
		dv->value = value;
		HASH_ADD_INT(di->data_value_hash, value, dv);
	}
	*/	
}

static inline void ski_stats_add_data_access(unsigned int data_address, unsigned int eip_address, unsigned int size_bytes, int is_write, unsigned int cpu_no){
//static inline void ski_stats_add_data_access(unsigned int data_address, unsigned int eip_address, unsigned int size_bytes, int is_write, unsigned int cpu_no, unsigned int value){
	int s;
    assert(size_bytes < SKI_STATS_MAX_ACCESS_SIZE);
    assert(cpu_no < SKI_STATS_MAX_CPU);

    //printf("ski_stats_add_data_access(): data_address %08x, eip_address %08x, size_bytes %d, is_write %d, cpu_no %d\n", data_address, eip_address, size_bytes, is_write, cpu_no);

	// XXX: It may be better to compute in a smarter way in the ski_stats_compute_communication function ?? Although it would still require a (data_address, size)-key indexing
	for(s=0;s<size_bytes;s++){
		ski_stats_add_data_access_byte(data_address + s, eip_address, is_write, cpu_no);
		//ski_stats_add_data_access_byte(data_address + s, eip_address, is_write, cpu_no, value);
	}
}


static inline void ski_stats_add_instruction_access(int eip_address, int cpu_no){
    
    ski_stats_instruction_accesses * ia;
    HASH_FIND_INT(ski_stats_self->instructions_hash, &eip_address, ia);

    if(ia == 0){
        // Address not there yet. Have to insert a new entry
        assert(ski_stats_self->instructions_n < SKI_STATS_MAX_DATA_ADDRESSES);
        ia = &ski_stats_self->instructions[ski_stats_self->instructions_n];
        ski_stats_self->instructions_n++;
        ia->eip_address = eip_address;
        ia->total_accesses = 0;
		ia->cpu_no = cpu_no;
        HASH_ADD_INT(ski_stats_self->instructions_hash, eip_address, ia);
    }

    ia->total_accesses++;
}


#define SKI_STATS_FINISH_SUCCESS() \
{															\
	ski_stats_get_self()->exit_code = 0;				\
    sprintf(ski_stats_get_self()->exit_reason, "%s", "SUCCESS");	\
    sprintf(ski_stats_get_self()->exit_location, "%s:%d", __FILE__, __LINE__);	\
}



#define SKI_STATS_ASSERT_LOG(assert_condition, code, description_str)		\
{																					\
    if(assert_condition){															\
        return;																		\
    }																				\
    ski_stats_get_self()->exit_code = code;									\
	sprintf(ski_stats_get_self()->exit_reason, "%s", description_str);			\
    sprintf(ski_stats_get_self()->exit_location, "%s:%d", __FILE__, __LINE__);	\
}



#endif

