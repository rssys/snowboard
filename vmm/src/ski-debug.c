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


#include <sys/time.h>
#include "config.h"
#include "cpu.h"
#include "disas.h"
#include "tcg.h"
#include "qemu-barrier.h"

#include "ski.h"
#include "sysemu.h"

#include "forkall-coop.h"
#include "ski-input.h"
#include "ski-race-detector.h"
#include "ski-memory-detector.h"
#include "ski-heuristics-statistics.h"

#include <libgen.h>
#include <stdio.h>

bool ski_trace_active;
FILE* ski_exec_trace_execution_fd;
FILE* ski_mem_write_fd;
FILE* ski_mem_read_fd;
int ski_exec_trace_nr_entries;

int ski_exec_instruction_counter_total = 0;
int ski_exec_instruction_counter_per_cycle = 0;

FILE* ski_control_preemption_fd = 0;
FILE* ski_control_parameters_fd = 0;
FILE* ski_control_coverage_fd = 0;
FILE* ski_race_detector_fd = 0;
FILE* ski_instruction_detector_fd = 0;
FILE* ski_memory_detector_fd = 0;
extern int ski_init_options_trace_write_set_enabled;
extern int ski_init_options_trace_read_set_enabled;
extern int ski_init_options_trace_exec_set_enabled;
extern int ski_init_options_set_cpu;

extern int ski_init_options_input_number[SKI_CPUS_MAX];
extern int ski_init_options_seed;
extern char ski_init_options_destination_dir[1024];
extern char ski_init_options_selective_trace_filename[1024];
extern int ski_init_options_race_detector_enabled;
extern int ski_init_options_debug_only_aggregate_output_enabled;

extern int ski_init_cpu1_preemption;
extern int ski_init_cpu2_preemption;
extern int ski_init_options_preemption_by_access;
extern int ski_init_options_short_profile;
void ski_exec_redirect_to_null(char* filename)
{
	if(ski_init_options_debug_only_aggregate_output_enabled){
		strcpy(filename, "/dev/null");
	}
}

void ski_exec_trace_start(CPUState* env){
    char trace_filename[512];
	char trace_filename_full[512];
	char trace_write_filename[1024];
	char trace_write_filename_full[1024];
	char trace_read_filename[1024];
	char trace_read_filename_full[1024];
    time_t t;
    struct tm *tmp;

    // Initialize the execution trace file for this test (different tests have different execution trace files)
    t = time(NULL);
    tmp = localtime(&t);
    assert(tmp);

	if(ski_forkall_enabled){
	    assert(snprintf(trace_filename, sizeof(trace_filename), "trace_%s_%d_%d_%d.txt", 
				ski_init_execution_ts, 
				ski_init_options_input_number[0] , ski_init_options_input_number[1], ski_init_options_seed));
		if (ski_init_options_short_profile){
			assert(snprintf(trace_write_filename, sizeof(trace_write_filename), "trace_%s_%d_%d_%d.txt.write", 
				ski_init_execution_ts, 
				ski_init_options_input_number[0] , ski_init_options_input_number[1], ski_init_options_seed));
			assert(snprintf(trace_read_filename, sizeof(trace_read_filename), "trace_%s_%d_%d_%d.txt.read", 
				ski_init_execution_ts, 
				ski_init_options_input_number[0] , ski_init_options_input_number[1], ski_init_options_seed));
		}
	}else{
	    assert(strftime(trace_filename, sizeof(trace_filename), "trace_%Y%m%d_%H%M%S.txt", tmp));
	}

    SKI_TRACE("Execution trace filename: \"%s\"\n", trace_filename);

	trace_filename_full[0] = 0;
	assert(strlen(ski_init_options_destination_dir)>0);
	sprintf(trace_filename_full, "%s/%s", ski_init_options_destination_dir, trace_filename);
	if (ski_init_options_short_profile){
		trace_write_filename_full[0] = 0;
		trace_read_filename_full[0] = 0;
		sprintf(trace_write_filename_full, "%s/%s", ski_init_options_destination_dir, trace_write_filename);
		sprintf(trace_read_filename_full, "%s/%s", ski_init_options_destination_dir, trace_read_filename);
	}

	ski_exec_redirect_to_null(trace_filename_full);
	//only actually open this file when SKI is neither under short_profiling mode or concurrent mode
	// When ski_init_options_short_profile is disabled, the trace_filename_full file should be opened
	if(ski_forkall_enabled){
		if (!ski_init_options_short_profile){
    		ski_exec_trace_execution_fd = fopen(trace_filename_full, "w");
			assert(ski_exec_trace_execution_fd);
    		SKI_TRACE("Opened the trace file\n");
		}
	}

	if (ski_forkall_enabled && ski_init_options_short_profile){
		ski_mem_write_fd = fopen(trace_write_filename_full, "w");
		ski_mem_read_fd = fopen(trace_read_filename_full, "w");
	}

	if(ski_forkall_enabled){
		ski_stats_add_trace(trace_filename, trace_filename_full);
	}

    ski_exec_trace_nr_entries = 0;
    ski_loop_init();
    tb_flush(env);
#define SKI_EXIT_OTHER -1
}

void ski_exec_trace_stop(CPUState* env){
    ski_loop_finish();
	if(ski_exec_trace_execution_fd){
        fclose(ski_exec_trace_execution_fd);
        ski_exec_trace_execution_fd = 0;
    }
	if (ski_mem_write_fd){
		fclose(ski_mem_write_fd);
		ski_mem_write_fd = 0;
	}
	if (ski_mem_read_fd){
		fclose(ski_mem_read_fd);
		ski_mem_read_fd = 0;
	}
	tb_flush(env);
	//FIXME: Should flush for all CPUs
}


//-------------------------------------------------------------------------------


FILE *ski_msg_trace_file = 0;

void ski_msg_trace_start(){
	char filename_full[1024];


    // Is this assert necessary?
	//assert(ski_forkall_enabled);
	assert(strlen(ski_init_options_destination_dir));

	assert(snprintf(filename_full, sizeof(filename_full), "%s/msg_%s_%d_%d_%d.txt",
		ski_init_options_destination_dir,
		ski_init_execution_ts,
		ski_init_options_input_number[0] , ski_init_options_input_number[1], ski_init_options_seed));

	ski_exec_redirect_to_null(filename_full);

    ski_msg_trace_file = fopen(filename_full,"w");
    assert(ski_msg_trace_file);

    SKI_TRACE_NOCPU("Opened the msg trace file\n");

}

void ski_msg_trace_stop(){
	if(ski_msg_trace_file){
		fclose(ski_msg_trace_file);
	}
}

void ski_msg_trace_print(int cpu, char *msg){
	printf("[SKI] [MSG] %s\n", msg);

	if(!ski_msg_trace_file){
		ski_msg_trace_start();
	}
	fprintf(ski_msg_trace_file, "[MSG] [CPU %d] %s\n", cpu, msg);
	fflush(ski_msg_trace_file); //XXX: Used by the testsuit-loader.sh script
}

//-------------------------------------------------------------------------------

FILE *ski_run_trace_file = 0;

void stark_run_trace_init(){
	assert(ski_run_trace_file==0);
	ski_run_trace_file = stdout;


}

void ski_run_trace_start(){
	char filename_full[1024];

	assert(ski_forkall_enabled);
	assert(ski_init_options_destination_dir);

	assert(snprintf(filename_full, sizeof(filename_full), "%s/run_%s_%d_%d_%d.txt",
		ski_init_options_destination_dir,
		ski_init_execution_ts,
		ski_init_options_input_number[0] , ski_init_options_input_number[1], ski_init_options_seed));

	ski_exec_redirect_to_null(filename_full);

    ski_run_trace_file = fopen(filename_full,"w");
    assert(ski_run_trace_file);

    SKI_TRACE_NOCPU("Opened the run trace file\n");

}

void ski_run_trace_stop(){
	if(ski_run_trace_file!=stdout){
		fclose(ski_run_trace_file);
		ski_run_trace_file=stdout;
	}
}

/*
void ski_run_trace_print(char *run){
	printf("[SKI] [RUN] %s\n", run);

	if(!ski_run_trace_file){
		ski_run_trace_start();
	}
	fprintf(ski_run_trace_file, "%s\n", run);
}*/


//-------------------------------------------------------------------------------


FILE *ski_st_trace_file = 0;

void ski_st_trace_start(){
	char filename_full[1024];

	if(strlen(ski_init_options_selective_trace_filename)==0){
		return;
	}

	assert(ski_forkall_enabled);
	assert(strlen(ski_init_options_destination_dir));

	assert(snprintf(filename_full, sizeof(filename_full), "%s/st_%s_%d_%d_%d.txt",
		ski_init_options_destination_dir,
		ski_init_execution_ts,
		ski_init_options_input_number[0] , ski_init_options_input_number[1], ski_init_options_seed));

    ski_st_trace_file = fopen(filename_full,"w");
    assert(ski_st_trace_file);

    SKI_TRACE_NOCPU("Opened the st trace file\n");

}

void ski_st_trace_stop(){
	if(ski_st_trace_file){
		fclose(ski_st_trace_file);
	}
}

void ski_st_trace_print(char *msg){
	if(!ski_st_trace_file){
		ski_st_trace_start();
	}
	fprintf(ski_st_trace_file, "%s\n", msg);
}

//-------------------------------------------------------------------------------

FILE *ski_heuristics_statistics_file = 0;

void ski_heuristics_statistics_start(){
	char filename_full[1024];

	assert(ski_forkall_enabled);
	assert(strlen(ski_init_options_destination_dir));

	assert(snprintf(filename_full, sizeof(filename_full), "%s/heuristics_statistics_%s_%d_%d_%d.txt",
		ski_init_options_destination_dir,
		ski_init_execution_ts,
		ski_init_options_input_number[0] , ski_init_options_input_number[1], ski_init_options_seed));

    ski_heuristics_statistics_file = fopen(filename_full,"w");
    assert(ski_heuristics_statistics_file);

    SKI_TRACE_NOCPU("Opened the heuristics statistics file\n");
}

void ski_heuristics_statistics_stop(){
	if(ski_heuristics_statistics_file){
		fclose(ski_heuristics_statistics_file);
		ski_heuristics_statistics_file = 0;
	}
}

void ski_heuristics_statistics_print(int heuristic_type, int instruction_address, int cpu_no, int instruction_counter){
	if(!ski_heuristics_statistics_file){
		ski_heuristics_statistics_start();
	}

	char* heuristic_type_str="NONE";

	switch(heuristic_type){
		case SKI_HEURISTICS_STATISTICS_HLT:
			heuristic_type_str = "HLT";
			break;
		case SKI_HEURISTICS_STATISTICS_PAUSE:
			heuristic_type_str = "PAUSE";
			break;
		case SKI_HEURISTICS_STATISTICS_LOOP:
			heuristic_type_str = "LOOP";
			break;
		case SKI_HEURISTICS_STATISTICS_STARVATION:
			heuristic_type_str = "STARVATION";
			break;
	}
	fprintf(ski_heuristics_statistics_file, "%s %x %d %d\n", heuristic_type_str, instruction_address, cpu_no, instruction_counter);
	fflush(0);
}


//-------------------------------------------------------------------------------

// General files use by the forkall parent process to write out information relevant for all the other thread
void ski_control_forkall_trace_start(){
    char trace_filename[256];
	char trace_filename_full[512];
	char ts[256];

    time_t t;
    struct tm *tmp;

    t = time(NULL);
    tmp = localtime(&t);
    assert(tmp);
	
	assert(ski_forkall_enabled);
	assert(strlen(ski_init_options_destination_dir)>0);

    assert(strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", tmp));

	sprintf(trace_filename, "forkall_preemption_points.txt");
    printf("[SKI] Control preemption trace filename: \"%s\"\n", trace_filename);
	sprintf(trace_filename_full, "%s/%s_%s", ski_init_options_destination_dir, ts, trace_filename);
    ski_control_preemption_fd = fopen(trace_filename_full,"wt");
    assert(ski_control_preemption_fd);

	sprintf(trace_filename, "forkall_parameters.txt");
    printf("[SKI] Control parameters trace filename: \"%s\"\n", trace_filename);
	sprintf(trace_filename_full, "%s/%s_%s", ski_init_options_destination_dir, ts, trace_filename);
    ski_control_parameters_fd = fopen(trace_filename_full,"wt");
    assert(ski_control_parameters_fd);

	sprintf(trace_filename, "forkall_coverage.txt");
    printf("[SKI] Control coverage trace filename: \"%s\"\n", trace_filename);
	sprintf(trace_filename_full, "%s/%s_%s", ski_init_options_destination_dir, ts, trace_filename);
    ski_control_coverage_fd = fopen(trace_filename_full,"wt");
    assert(ski_control_coverage_fd);

	if(ski_init_options_race_detector_enabled){
		sprintf(trace_filename, "forkall_race_detector.txt");
		printf("[SKI] Races trace filename: \"%s\"\n", trace_filename);
		sprintf(trace_filename_full, "%s/%s_%s", ski_init_options_destination_dir, ts, trace_filename);
		ski_race_detector_fd = fopen(trace_filename_full,"wt");
		assert(ski_race_detector_fd);
	}

	if(strlen(ski_init_options_instructions_detector_filename)>=0){
		sprintf(trace_filename, "forkall_instruction_detector.txt");
		printf("[SKI] Instruction trace filename: \"%s\"\n", trace_filename);
		sprintf(trace_filename_full, "%s/%s_%s", ski_init_options_destination_dir, ts, trace_filename);
		ski_instruction_detector_fd = fopen(trace_filename_full,"wt");
		assert(ski_instruction_detector_fd);
	}


	#ifdef SKI_MEMORY_DETECTOR_ENABLED
	sprintf(trace_filename, "forkall_memory_detector.txt");
	printf("[SKI] Memory trace filename: \"%s\"\n", trace_filename);
	sprintf(trace_filename_full, "%s/%s_%s", ski_init_options_destination_dir, ts, trace_filename);
	ski_memory_detector_fd = fopen(trace_filename_full,"wt");
	assert(ski_memory_detector_fd);
	#endif


    printf("[SKI] Finished opening the trace files\n");

}


void ski_rd_print(ski_stats *stats){
	if(ski_init_options_race_detector_enabled){
		printf("[SKI] Printing race detector results\n");
		char *trace_filename = stats->trace_filename[0] ? stats->trace_filename : "???";
		ski_race_detector_print(&stats->rd, trace_filename, stats->seed, stats->input_number[0], stats->input_number[1], ski_race_detector_fd);
	}
}

void ski_id_print(ski_stats *stats){
	if(strlen(ski_init_options_instructions_detector_filename)>=0){
		printf("[SKI] Printing instruction detector results\n");
		char *trace_filename = stats->trace_filename[0] ? stats->trace_filename : "???";
		ski_instruction_detector_print(&stats->id, stats->trace_filename, stats->seed, stats->input_number[0], stats->input_number[1], ski_instruction_detector_fd);
	}
}

void ski_md_print(ski_stats *stats){
	#ifdef SKI_MEMORY_DETECTOR_ENABLED
	if(stats->exit_code != SKI_EXIT_WATCHDOG){
		printf("[SKI] Printing memory detector results\n");
		char *trace_filename = stats->trace_filename[0] ? stats->trace_filename : "???";
		ski_memory_detector_print(&stats->md, stats->trace_filename, stats->seed, stats->input_number[0], stats->input_number[1], ski_memory_detector_fd);
	}else{
		printf("[SKI] Not printing memory detector results because of the exit code (%s)\n", stats->exit_reason);
	}
	#endif
}


void ski_control_parameters_print(ski_stats *stats){
	struct timeval diff;
	int i;


	ski_forkall_timeval_subtract(&diff, &stats->finish, &stats->start);

	char *trace_filename = stats->trace_filename[0] ? stats->trace_filename : "???";
	char *exit_reason = stats->exit_reason[0] ? stats->exit_reason : "???";
	char exit_location[128];
	char *exit_location_basename;

	sprintf(exit_location, "%s", stats->exit_location);
	exit_location_basename = basename(exit_location);

	// Generate a string for this information
	char preemptions[1024];
	preemptions[0] = 0;
	for(i=0;i<stats->preemptions_len;i++){
		char tmp[56];
		sprintf(tmp,"%d,", stats->preemptions[i]);
		strcat(preemptions, tmp);
		assert(strlen(preemptions)<1000);
	}
	
	// We should also print the communication address
	assert(ski_control_parameters_fd);
	fprintf(ski_control_parameters_fd, "T: %s S: %d I1: %d I2: %d Slot: %d Pid: %d P_size: %d P_points: %s P_exec: %d C_data: %d C_eips: %d Dur: %d.%06d TI: %lld #I: %lld #D: %lld Exit: %s %s Channel: %d Communicate: %d Wendpoint: %08x Rendpoint: %08x C_ADDR: %08x\n", 
		trace_filename, 
		stats->seed, stats->input_number[0], stats->input_number[1], stats->slot, stats->pid, stats->preemption_list_size, preemptions, stats->preemption_points_executed, stats->communication_data, stats->communication_eips,
		diff.tv_sec, diff.tv_usec,
		stats->instructions_executed,
		stats->instructions_n, stats->data_n,
		exit_reason, 
		exit_location_basename,
		stats->channel_hit,
		stats->communicate,
		stats->ski_write_endpoint,
		stats->ski_read_endpoint,
		stats->ski_communication_addr);
}


void ski_control_preemptionlist_print(int eip_address){
	if(!ski_forkall_enabled)
		return;

	assert(ski_control_preemption_fd);
	fprintf(ski_control_preemption_fd, "%x\n", eip_address);
}

void ski_exec_trace_flush(void){
    //	SKI: test temporarily removed the fflush to reduce the number of system calls
	//fflush(0);

    //fsync(0); Invalid argument
    //sync();  // THis would take an extra second or so, mostly because of NFS and other systems
}

void ski_exec_trace_print_comment(char *comment){
    if(ski_exec_trace_execution_fd && (ski_exec_trace_nr_entries < SKI_EXEC_TRACE_MAX_ENTRIES)){
        fprintf(ski_exec_trace_execution_fd, "### %s\n", comment);
        // No need to increment the ski_exec_trace_nr_entries for comments
    }
}
extern int ski_init_options_trace_instructions_enabled;
void ski_exec_trace_print_initial_comment(CPUState* env){
    char str[512];
    char str2[512];
    time_t t;
    struct tm *tmp;

    t = time(NULL);
    tmp = localtime(&t);
    assert(tmp);
    strftime(str2, sizeof(str2), "%Y-%m-%d %H:%M:%S", tmp);

    sprintf(str, "Date: %s CPUs: %d, nr_instructions: %d", str2, env->ski_cpu.nr_cpus, env->ski_cpu.nr_max_instr);
    if (ski_init_options_trace_instructions_enabled)
		ski_exec_trace_print_comment(str);

    sprintf(str2, "Wrote comment: \"%s\"\n", str);
    SKI_TRACE(str2);
	
	//XXX: Should check the sizes to avoid buffer overflows here...
	//XXX: Ugly code...improve...
	assert((strlen(ski_init_kernel_filename) + strlen(ski_init_kernel_sha1) + 20) < 250);

	sprintf(str, "Kernel_filename: %s kernel_hash: %s kernel_size: %d",
		ski_init_kernel_filename, ski_init_kernel_sha1, ski_init_kernel_size);
	if (ski_init_options_trace_instructions_enabled)
    	ski_exec_trace_print_comment(str);
    sprintf(str2, "Wrote comment: \"%s\"\n", str);
    SKI_TRACE(str2);

	assert((strlen(ski_init_disk_filename) + strlen(ski_init_disk_sha1) + 20) < 250);
	sprintf(str, "Disk_filename: %s disk_hash: %s disk_size: %d",
		ski_init_disk_filename, ski_init_disk_sha1, ski_init_disk_size);
    if (ski_init_options_trace_instructions_enabled)
		ski_exec_trace_print_comment(str);
    sprintf(str2, "Wrote comment: \"%s\"\n", str);
    SKI_TRACE(str2);

    //TODO: May want to hash during boting the kernel image (if using the QEMU method) and write it down
}

void ski_debug_print_config(CPUState* env){
	// Print all the configuration options from ski-config.h
	SKI_TRACE("ski_debug_print_config():\n");
	SKI_TRACE("  SKI_EXEC_TRACE_MAX_ENTRIES = %d\n", SKI_EXEC_TRACE_MAX_ENTRIES);
	SKI_TRACE("  SKI_SCHED_MAX_PREEMPTION_INSTRUCTIONS = %d\n", SKI_SCHED_MAX_PREEMPTION_INSTRUCTIONS);
	SKI_TRACE("  SKI_SCHED_ADJUST_MODULO = %d\n", SKI_SCHED_ADJUST_MODULO);
	SKI_TRACE("  SKI_SCHED_ADJUST_INC = %d\n", SKI_SCHED_ADJUST_INC);
	// TODO: Add the rest of options if they exist

}

void ski_debug_memory(CPUState* env){
    char* addr;
    char buf[8];
    int i;

    SKI_TRACE("ski_debug_memory\n");
    SKI_TRACE("ski_debug_memory IDT.base=%x IDT.limit=%x\n", env->idt.base, env->idt.limit);

    for(addr = env->idt.base ; addr < env->idt.base + env->idt.limit ; addr+=sizeof(buf)){
        int len = MIN((env->idt.base + env->idt.limit) - env->idt.base, sizeof(buf));
        if(len>=8){

            cpu_memory_rw_debug(env,addr, buf, sizeof(buf), 0);
            //cpu_physical_memory_rw(addr, buf, sizeof(buf), 0);
            //SKI_TRACE("IDT i: %02d %8x %8x\n", i, e1, e2);  
            SKI_TRACE("IDT i: %02d %8x %8x\n", i, ((int*)buf)[0],((int*)buf)[1]);
        }
        i++;
    }
}

void ski_debug(){
	CPUState *env = first_cpu;
	CPUState *penv = first_cpu;

	SKI_TRACE("=========SKI DEBUG START=======\n");
	SKI_TRACE("ski_debug\n");
	ski_threads_dump(env);
	while (penv){
		if(penv->ski_active){
			SKI_TRACE("ski_debug (CPU %d)\n", penv->cpu_index);
			ski_interrupts_dump_stack(penv);
		}
		penv = penv->next_cpu;
	}
	SKI_TRACE("ski_threads_current (%x):\n", ski_threads_current);
	if(ski_threads_current){
		ski_tc_dump(env, ski_threads_current);
	}
	SKI_TRACE("=========SKI DEBUG END=========\n");
}
