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


#include "forkall-coop.h"
#include "ski-stats.h"
#include "ski-debug.h"
#include "ski-input.h"
#include "ski-config.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <time.h>



extern int ski_init_options_seed;

/*
INPUTS:
	SKI_INPUT1_RANGE=
	SKI_INPUT2_RANGE=
*/

typedef struct struct_ski_input_int_range{
	long long int range[MAX_SKI_INTPUT_INT_RANGE];
	int range_n;
	int iterator;
	int relative;
	int from_file_single;
	int from_file_pairs;
} ski_input_int_range;

ski_input_int_range ski_init_options_input1_range;
ski_input_int_range ski_init_options_input2_range;
ski_input_int_range ski_init_options_interleaving_range;
ski_input_int_range ski_init_options_cpu1_preemption;
ski_input_int_range ski_init_options_cpu2_preemption;
ski_input_int_range ski_init_options_preemption_channel_addr;
ski_input_int_range ski_init_options_cpu1_preemption_value;
ski_input_int_range ski_init_options_cpu2_preemption_value;

char ski_init_execution_ts[256];

int ski_input_file[SKI_INPUT_FILE_MAX];
int ski_input_file_n = 0;

int ski_input_pairs_file[SKI_INPUT_PAIRS_FILE_MAX][2];
int ski_input_pairs_file_n = 0;
int pair_mode=0;


extern int ski_init_options_input_number[SKI_CPUS_MAX];
extern char ski_init_options_destination_dir[1024];
extern char ski_init_options_instructions_detector_filename[1024];
extern char ski_init_options_selective_trace_filename[1024];
extern char ski_init_options_input_filename[1024];
extern char ski_init_options_input_pairs_filename[1024];
extern int ski_init_options_watchdog_seconds;
extern int ski_init_options_heuristics_statistics_enabled;
extern int ski_init_options_debug_child_wait_start_seconds;
extern int ski_init_options_debug_parent_executes_enabled;
extern int ski_init_options_dir_per_input_enabled;
extern int ski_init_options_debug_exit_after_hypercall_enabled;
extern int ski_init_options_race_detector_enabled;
extern int ski_init_options_debug_only_aggregate_output_enabled;

extern int ski_init_options_trace_instructions_enabled;
extern int ski_init_options_trace_memory_accesses_enabled;
extern int ski_init_options_trace_write_set_enabled;
extern int ski_init_options_trace_read_set_enabled;
extern int ski_init_options_trace_exec_set_enabled;
extern int ski_init_options_set_cpu;

extern char* ski_init_options_priorities_filename;
extern char* ski_init_options_ipfilter_filename;
extern int ski_init_options_forkall_preemption_points;
extern int ski_init_options_forkall_k_initial;
extern int ski_init_options_quit_hypercall_threshold;

extern int ski_init_options_preemption_by_eip;
extern int ski_init_options_short_profile;
// Snowboard scheduling scheme
extern int ski_init_options_preemption_by_access;
extern int ski_init_cpu1_preemption;
extern int ski_init_cpu2_preemption;
extern int ski_init_preemption_channel_addr;
extern int ski_init_cpu1_preemption_value;
extern int ski_init_cpu2_preemption_value;
extern int ski_init_options_cpu_1_mode;
extern int ski_init_options_cpu_2_mode;
extern char ski_init_options_corpus_path[1024];

void ski_input_env_load_str(char *env_name, char *variable);
void ski_input_env_load_int_range(char *env_name, ski_input_int_range *variable);
void ski_input_env_load_int(char *env_name, int *variable_out);
void ski_input_init_execution_ts(void);
static void recursive_mkdir(const char *dir);
static void ski_input_load_input_file(void);

static void ski_input_load_input_file(void)
{
    unsigned int input_number;
	FILE *fp;

	if(strlen(ski_init_options_input_filename)==0){
		return;
	}

	fp = fopen(ski_init_options_input_filename, "r");
	if(!fp){
		printf("[SKI] Error: Unable to open input file: %s\n", ski_init_options_input_filename);
		assert(0);
	}	

    while(1){
        int res;
        char buffer[512];

        res = fgets(buffer, 512-1, fp);
        if(!res){
            break;
        }
        if(buffer[strlen(buffer)-1]=='\n')
            buffer[strlen(buffer)-1] = 0;

        res = sscanf(buffer, "%d", &input_number);
        if(res != 1){
            break;
        }

		ski_input_file[ski_input_file_n] = input_number;
		ski_input_file_n++;
		assert(ski_input_file_n < SKI_INPUT_FILE_MAX);
    }

    printf("[SKI] ski_input_load_input_file: Loaded %d input numbers\n", ski_input_file_n);
    assert(ski_input_file_n);
}

static void ski_input_load_input_pairs_file(void)
{
    unsigned int input1_number;
    unsigned int input2_number;
	FILE *fp;

	if(strlen(ski_init_options_input_pairs_filename)==0){
		return;
	}

	fp = fopen(ski_init_options_input_pairs_filename, "r");
	if(!fp){
		printf("[SKI] Error: Unable to open input pairs file: %s\n", ski_init_options_input_pairs_filename);
		assert(0);
	}	

    while(1){
        int res;
        char buffer[512];

        res = fgets(buffer, 512-1, fp);
        if(!res){
            break;
        }
        if(buffer[strlen(buffer)-1]=='\n')
            buffer[strlen(buffer)-1] = 0;

        res = sscanf(buffer, "%d %d", &input1_number, &input2_number);
        if(res != 2){
            break;
        }

		ski_input_pairs_file[ski_input_pairs_file_n][0] = input1_number;
		ski_input_pairs_file[ski_input_pairs_file_n][1] = input2_number;
		ski_input_pairs_file_n++;
		assert(ski_input_pairs_file_n < SKI_INPUT_PAIRS_FILE_MAX);
    }

    printf("[SKI] ski_input_load_input_pairs_file: Loaded %d input pairs\n", ski_input_pairs_file_n);
    assert(ski_input_pairs_file_n);
}


// Called from the master process during initialization (before the fork)
long long int ski_input_init(void)
{
	
	ski_input_env_load_int_range("SKI_INPUT1_RANGE", &ski_init_options_input1_range);
	ski_input_env_load_int_range("SKI_INPUT2_RANGE", &ski_init_options_input2_range);
	ski_input_env_load_int_range("SKI_INTERLEAVING_RANGE", &ski_init_options_interleaving_range);

	assert(ski_init_options_input1_range.range_n > 0);
	assert(ski_init_options_input2_range.range_n > 0);
	assert(ski_init_options_interleaving_range.range_n > 0);

	// Only input 2 many have the relative flag (because it's relative to input1)
	assert(ski_init_options_input1_range.relative == 0);
	assert(ski_init_options_interleaving_range.relative == 0);

	// Input 2 can only refer to the input file if input 1 also refers to the input file 
	if(ski_init_options_input1_range.from_file_single == 0)
		assert(ski_init_options_input2_range.from_file_single == 0);
	assert(ski_init_options_input1_range.from_file_pairs == ski_init_options_input2_range.from_file_pairs);
	assert(ski_init_options_interleaving_range.from_file_single == 0);
	assert(ski_init_options_interleaving_range.from_file_pairs == 0);
	
	long long int total_tests=0;
	if (pair_mode == 0)
		total_tests =  ski_init_options_input1_range.range_n * ski_init_options_input2_range.range_n * ski_init_options_interleaving_range.range_n;
	else 
		total_tests =  ski_init_options_input1_range.range_n * ski_init_options_interleaving_range.range_n;
		
	printf("[SKI] ski_input_init: Executing %d tests\n", total_tests);
	if(total_tests>1000*1000){
		printf("[SKI] WARNING: Many tests scheduled\n", total_tests);
	}
	ski_input_env_load_str("SKI_CORPUS_DIR", &ski_init_options_corpus_path);

	ski_input_env_load_str("SKI_OUTPUT_DIR", &ski_init_options_destination_dir);
	assert(strlen(ski_init_options_destination_dir)>0);
	recursive_mkdir(ski_init_options_destination_dir);

	ski_input_env_load_int("SKI_OUTPUT_DIR_PER_INPUT_ENABLED", &ski_init_options_dir_per_input_enabled);
	assert(ski_init_options_dir_per_input_enabled>=0);

	ski_input_env_load_int("SKI_WATCHDOG_SECONDS", &ski_init_options_watchdog_seconds);
	assert(ski_init_options_watchdog_seconds>=0);

	ski_input_env_load_int("SKI_RACE_DETECTOR_ENABLED", &ski_init_options_race_detector_enabled);
	assert(ski_init_options_race_detector_enabled>=0);

	ski_input_env_load_str("SKI_INSTRUCTIONS_DETECTOR_FILENAME", &ski_init_options_instructions_detector_filename);

	ski_input_env_load_str("SKI_SELECTIVE_TRACE_FILENAME", &ski_init_options_selective_trace_filename);

	ski_input_env_load_int("SKI_HEURISTICS_STATISTICS_ENABLED", &ski_init_options_heuristics_statistics_enabled);

	ski_input_env_load_str("SKI_INPUT_FILENAME", &ski_init_options_input_filename);
	assert((ski_init_options_input1_range.from_file_single == 0) || (strlen(ski_init_options_input_filename) > 0));
	ski_input_load_input_file();

	ski_input_env_load_str("SKI_INPUT_PAIRS_FILENAME", &ski_init_options_input_pairs_filename);
	assert((ski_init_options_input1_range.from_file_pairs == 0) || (strlen(ski_init_options_input_pairs_filename) > 0));
	assert(strlen(ski_init_options_input_filename) == 0 || strlen(ski_init_options_input_pairs_filename) == 0);
	ski_input_load_input_pairs_file();

	ski_input_env_load_int("SKI_DEBUG_ONLY_AGGREGATE_OUTPUT_ENABLED", &ski_init_options_debug_only_aggregate_output_enabled);
	assert(&ski_init_options_debug_only_aggregate_output_enabled>=0);

	ski_input_env_load_int("SKI_DEBUG_CHILD_WAIT_START_SECONDS", &ski_init_options_debug_child_wait_start_seconds);
	assert(ski_init_options_debug_child_wait_start_seconds>=0);

	ski_input_env_load_int("SKI_DEBUG_PARENT_EXECUTES_ENABLED", &ski_init_options_debug_parent_executes_enabled);
	assert((ski_init_options_debug_parent_executes_enabled == 0) || (ski_init_options_debug_parent_executes_enabled == 1));

	ski_input_env_load_int("SKI_DEBUG_EXIT_AFTER_HYPERCALL_ENABLED", &ski_init_options_debug_exit_after_hypercall_enabled);
	assert((ski_init_options_debug_exit_after_hypercall_enabled == 0) || (ski_init_options_debug_exit_after_hypercall_enabled == 1));

	ski_input_env_load_str("SKI_IPFILTER_FILENAME", &ski_init_options_ipfilter_filename);
	ski_input_env_load_str("SKI_PRIORITIES_FILENAME", &ski_init_options_priorities_filename);
	ski_input_env_load_int("SKI_FORKALL_ENABLED", &ski_forkall_enabled);
	ski_input_env_load_int("SKI_FORKALL_CONCURRENCY",&ski_init_options_forkall_concurrency);
	assert(ski_init_options_forkall_concurrency>0);
	// enhance the forkall_concurrency limit
	assert(ski_init_options_forkall_concurrency<256);
	ski_input_env_load_int("SKI_RESCHEDULE_POINTS",&ski_init_options_forkall_preemption_points);
	ski_input_env_load_int("SKI_RESCHEDULE_K",&ski_init_options_forkall_k_initial);
	ski_input_env_load_int("SKI_QUIT_HYPERCALL_THRESHOLD",&ski_init_options_quit_hypercall_threshold);

	ski_input_env_load_int("SKI_DEBUG_ASSERT_TRAP_ENABLED", &ski_init_options_debug_assert_trap_enabled);

	ski_input_env_load_int("SKI_TRACE_INSTRUCTIONS_ENABLED", &ski_init_options_trace_instructions_enabled);
	ski_input_env_load_int("SKI_TRACE_MEMORY_ACCESSES_ENABLED", &ski_init_options_trace_memory_accesses_enabled);
	// to-do
	ski_input_env_load_int("SKI_TRACE_WRITE_SET_ENABLED", &ski_init_options_trace_write_set_enabled);
	ski_input_env_load_int("SKI_TRACE_READ_SET_ENABLED", &ski_init_options_trace_read_set_enabled);
	ski_input_env_load_int("SKI_TRACE_EXEC_SET_ENABLED", &ski_init_options_trace_exec_set_enabled);
	ski_input_env_load_int("SKI_TRACE_SET_CPU", &ski_init_options_set_cpu);
	if (ski_init_options_trace_write_set_enabled){
		assert(ski_init_options_set_cpu>=0);
	}

	ski_input_env_load_int("SKI_TEST_CPU_1_MODE", &ski_init_options_cpu_1_mode);
	ski_input_env_load_int("SKI_TEST_CPU_2_MODE", &ski_init_options_cpu_2_mode);
	ski_input_env_load_int("SKI_PREEMPTION_BY_EIP", &ski_init_options_preemption_by_eip);
	ski_input_env_load_int("SKI_SHORT_PROFILE", &ski_init_options_short_profile);
	ski_input_env_load_int("SKI_PREEMPTION_BY_ACCESS", &ski_init_options_preemption_by_access);
	if (ski_init_options_preemption_by_access){
		printf("============================== SKI debug ==============================\n");
		printf("Reading preemption list\n");
		printf("============================== SKI debug ==============================\n");
		ski_input_env_load_int_range("SKI_CPU1_PREEMPTION", &ski_init_options_cpu1_preemption);
		ski_input_env_load_int_range("SKI_CPU2_PREEMPTION", &ski_init_options_cpu2_preemption);
		ski_input_env_load_int_range("SKI_CPU1_PREEMPTION_VALUE", &ski_init_options_cpu1_preemption_value);
		ski_input_env_load_int_range("SKI_CPU2_PREEMPTION_VALUE", &ski_init_options_cpu2_preemption_value);
	}
	if (ski_init_options_preemption_by_access){
		printf("============================== SKI debug ==============================\n");
		printf("Reading channel address\n");
		printf("============================== SKI debug ==============================\n");
		ski_input_env_load_int_range("SKI_CHANNEL_ADDR", &ski_init_options_preemption_channel_addr);
	}

#ifdef SKI_DISABLE_TRACE_INSTRUMENTATION
	assert(ski_init_options_trace_instructions_enabled == 0);
	assert(ski_init_options_trace_memory_accesses_enabled == 0);
#endif


	return total_tests;
}

long long int ski_input_get_current(ski_input_int_range *range, int input_no){
	long long int range_value;
	
	assert(input_no == 0 || input_no == 1);

	range_value = range->range[range->iterator];
	
	if(range->from_file_single){
		assert(ski_input_file_n > 0);

		if(range_value >= ski_input_file_n)
			range_value = ski_input_file_n - 1;
		range_value = ski_input_file[range_value];

	}else if(range->from_file_pairs){
		assert(ski_input_pairs_file_n > 0);

		if(range_value >= ski_input_pairs_file_n)
			range_value = ski_input_pairs_file_n - 1;
		range_value = ski_input_pairs_file[range_value][input_no];
	}
	return range_value;
}


/*
There are three parameter cases:
  Case 1: @ +     final input 2 = F[input1] + input2 
  Case 2: @ +@    final input 2 = F[input1 + input2]
  Case 3:   +     final input 2 = input1 + input2
*/

long long int ski_input_get_current_relative(ski_input_int_range *range1, ski_input_int_range * range2){
	long long int res;
	

	assert(range2->relative);
	assert(!range1->relative);
	
	if(range1->from_file_single){
		long long int file_index;
		assert(ski_input_file_n > 0);
		if(!range2->from_file_single){
			// Case 1
			file_index = range1->range[range1->iterator];
			file_index = file_index % ski_input_file_n;
			res = ski_input_file[file_index] + range2->range[range2->iterator];
		}else{
			// Case 2
			file_index = range1->range[range1->iterator] + range2->range[range2->iterator];
			file_index = file_index % ski_input_file_n;
			res = ski_input_file[file_index];
		}
	}else{
		// Case 3
		res = range1->range[range1->iterator] + range2->range[range2->iterator];
	}

	return res;
}

int ski_input_fork_skip_init(){
	
	// Move to the next input pair directly
	ski_init_options_interleaving_range.iterator = 0;
	ski_init_options_input2_range.iterator++;
	ski_init_options_input1_range.iterator++;
	if (ski_init_options_preemption_by_access){
		// every pair should have one preemption
		ski_init_options_cpu1_preemption.iterator++;
		ski_init_options_cpu2_preemption.iterator++;
		ski_init_options_cpu1_preemption_value.iterator++;
		ski_init_options_cpu2_preemption_value.iterator++;
		}
	if (ski_init_options_preemption_by_access){
		ski_init_options_preemption_channel_addr.iterator++;
	}
	if(ski_init_options_input1_range.iterator >= ski_init_options_input1_range.range_n){
		// We've reached the end, no more point in the space to explore
		return -1;
	}
	// Update the variables used by the child processes
	ski_init_options_input_number[0] = ski_input_get_current(&ski_init_options_input1_range, 0);
	if(ski_init_options_input2_range.relative == 0){
		if(ski_init_options_input1_range.from_file_pairs){
			// When reading pairs from the file: Get the iterator from input1 to figure out the value of input2 
			ski_init_options_input_number[1] = ski_input_get_current(&ski_init_options_input1_range, 1);
		}else{
			ski_init_options_input_number[1] = ski_input_get_current(&ski_init_options_input2_range, 1);
		}
	}else{
		// Relative flag for input2
		ski_init_options_input_number[1] = ski_input_get_current_relative(&ski_init_options_input1_range, &ski_init_options_input2_range);
	}
	if (ski_init_options_preemption_by_access){
		ski_init_cpu1_preemption = ski_input_get_current(&ski_init_options_cpu1_preemption, 0);
		ski_init_cpu2_preemption = ski_input_get_current(&ski_init_options_cpu2_preemption, 0);
		ski_init_cpu1_preemption_value = ski_input_get_current(&ski_init_options_cpu1_preemption_value, 0);
		ski_init_cpu2_preemption_value = ski_input_get_current(&ski_init_options_cpu2_preemption_value, 0);
		//printf("============================== SKI skip function debug ==============================\n");
		//printf("SKI_CPU1_preemption %08x\n", ski_init_cpu1_preemption);
		//printf("SKI_CPU2_preemption %08x\n", ski_init_cpu2_preemption);
		//printf("============================== SKI skip function debug ==============================\n");
	}
	if (ski_init_options_preemption_by_access){
		ski_init_preemption_channel_addr = ski_input_get_current(&ski_init_options_preemption_channel_addr, 0);
		//printf("============================== SKI skip function debug ==============================\n");
		//printf("SKI_CPU1_preemption %08x\n", ski_init_preemption_channel_addr);
		//printf("============================== SKI skip function debug ==============================\n");
	}
	ski_init_options_seed = ski_init_options_interleaving_range.range[ski_init_options_interleaving_range.iterator];
	//printf("============================== SKI skip function debug ==============================\n");
	
	printf("[SKI] Next child will explore inputs %lld and %lld and interleaving: %lld\n", 
		ski_init_options_input1_range.range[ski_init_options_input1_range.iterator],
		ski_init_options_input2_range.range[ski_init_options_input2_range.iterator],
		ski_init_options_interleaving_range.range[ski_init_options_interleaving_range.iterator]);
	//printf("============================== SKI skip function debug ==============================\n");
	
	if(ski_init_options_interleaving_range.iterator==0){
		// New input

		//XXX: Improve: slow and not nice...
		if((ski_init_options_dir_per_input_enabled) && (ski_init_options_debug_only_aggregate_output_enabled==0)){
			char env_ski_output_dir[1024];
			ski_input_env_load_str("SKI_OUTPUT_DIR", &env_ski_output_dir);
			sprintf(ski_init_options_destination_dir, "%s/%d_%d/", env_ski_output_dir, ski_init_options_input_number[0], ski_init_options_input_number[1]);
			recursive_mkdir(ski_init_options_destination_dir);
		}
	}
	ski_init_options_interleaving_range.iterator++;
	ski_input_init_execution_ts();
	
}

// Effects: Updates the current interleaving number
// Effects: Updates the current input numbers
// Called from the master process multiple times just before each fork (to set up the state for the child processes)
void ski_input_fork_init(int *out_new_input)
{
	*out_new_input = 0;

	if(ski_init_options_input1_range.iterator >= ski_init_options_input1_range.range_n){
		// We've reached the end, no more point in the space to explore
		return;
	}

	// Update the variables used by the child processes
	ski_init_options_input_number[0] = ski_input_get_current(&ski_init_options_input1_range, 0);
	if(ski_init_options_input2_range.relative == 0){
		if(ski_init_options_input1_range.from_file_pairs){
			// When reading pairs from the file: Get the iterator from input1 to figure out the value of input2 
			ski_init_options_input_number[1] = ski_input_get_current(&ski_init_options_input1_range, 1);
		}else{
			ski_init_options_input_number[1] = ski_input_get_current(&ski_init_options_input2_range, 1);
		}
	}else{
		// Relative flag for input2
		ski_init_options_input_number[1] = ski_input_get_current_relative(&ski_init_options_input1_range, &ski_init_options_input2_range);
	}
	if (ski_init_options_preemption_by_access){
		ski_init_cpu1_preemption = ski_input_get_current(&ski_init_options_cpu1_preemption, 0);
		ski_init_cpu2_preemption = ski_input_get_current(&ski_init_options_cpu2_preemption, 0);
		ski_init_cpu1_preemption_value = ski_input_get_current(&ski_init_options_cpu1_preemption_value, 0);
		ski_init_cpu2_preemption_value = ski_input_get_current(&ski_init_options_cpu2_preemption_value, 0);
		printf("============================== SKI debug ==============================\n");
		printf("SKI_CPU1_preemption %08x\n", ski_init_cpu1_preemption);
		printf("SKI_CPU2_preemption %08x\n", ski_init_cpu2_preemption);
		printf("============================== SKI debug ==============================\n");
	}
	if (ski_init_options_preemption_by_access){
		ski_init_preemption_channel_addr = ski_input_get_current(&ski_init_options_preemption_channel_addr, 0);
		printf("============================== SKI debug ==============================\n");
		printf("SKI_CPU1_preemption %08x\n", ski_init_preemption_channel_addr);
		printf("============================== SKI debug ==============================\n");
	}
	ski_init_options_seed = ski_init_options_interleaving_range.range[ski_init_options_interleaving_range.iterator];

	printf("[SKI] Next child will explore inputs %lld and %lld and interleaving: %lld\n", 
		ski_init_options_input1_range.range[ski_init_options_input1_range.iterator],
		ski_init_options_input2_range.range[ski_init_options_input2_range.iterator],
		ski_init_options_interleaving_range.range[ski_init_options_interleaving_range.iterator]);

	if(ski_init_options_interleaving_range.iterator==0){
		// New input

		// Inform the caller that we're going to explore a different input
		*out_new_input = 1;
		
		// 
		if((ski_init_options_forkall_concurrency*4) > ski_init_options_interleaving_range.range_n){
			printf("[SKI] WARNING: number of interleaving (%d) should be higher than the number of parallel executions (%d)\n", 
				ski_init_options_interleaving_range.range_n, ski_init_options_forkall_concurrency);
		}

		//XXX: Improve: slow and not nice...
		if((ski_init_options_dir_per_input_enabled) && (ski_init_options_debug_only_aggregate_output_enabled==0)){
			char env_ski_output_dir[1024];
			ski_input_env_load_str("SKI_OUTPUT_DIR", &env_ski_output_dir);
			sprintf(ski_init_options_destination_dir, "%s/%d_%d/", env_ski_output_dir, ski_init_options_input_number[0], ski_init_options_input_number[1]);
			recursive_mkdir(ski_init_options_destination_dir);
		}

	}

	// Conceptually we're doing:
	//  For each input1
	//     For each input2
	//        For each interleaving

	ski_init_options_interleaving_range.iterator++;
	if (pair_mode == 0){
		if(ski_init_options_interleaving_range.iterator >= ski_init_options_interleaving_range.range_n){
			ski_init_options_interleaving_range.iterator = ski_init_options_interleaving_range.iterator % ski_init_options_interleaving_range.range_n;

			ski_init_options_input2_range.iterator++;

			if((ski_init_options_input2_range.iterator >= ski_init_options_input2_range.range_n) || ski_init_options_input1_range.from_file_pairs){
				ski_init_options_input2_range.iterator = ski_init_options_input2_range.iterator % ski_init_options_input2_range.range_n;

				ski_init_options_input1_range.iterator++;
			}
		}
	}
	else{
		if(ski_init_options_interleaving_range.iterator >= ski_init_options_interleaving_range.range_n){
			ski_init_options_interleaving_range.iterator = ski_init_options_interleaving_range.iterator % ski_init_options_interleaving_range.range_n;

			ski_init_options_input2_range.iterator++;

			ski_init_options_input1_range.iterator++;
			if (ski_init_options_preemption_by_access){
				// every pair should have one preemption
				ski_init_options_cpu1_preemption.iterator++;
				ski_init_options_cpu2_preemption.iterator++;
				ski_init_options_cpu1_preemption_value.iterator++;
				ski_init_options_cpu2_preemption_value.iterator++;
				}
			if (ski_init_options_preemption_by_access){
				ski_init_options_preemption_channel_addr.iterator++;
			}
		}
	}
	
	ski_input_init_execution_ts();

	return;
}


void ski_input_init_execution_ts(void)
{
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	tmp = localtime(&t);
	assert(tmp);

	assert(strftime(ski_init_execution_ts, sizeof(ski_init_execution_ts), "%Y%m%d_%H%M%S", tmp));
}

void ski_input_env_load_int(char *env_name, int *variable_out){
    char *env_str;
    env_str = getenv(env_name);
    if(env_str && (strlen(env_str)>0)){
        int res = sscanf(env_str, "%d", variable_out);
        assert(res == 1);
    } else {
        // Env. variable not set or empty
        *variable_out = 0;
    }
	printf("[SKI] %s=%d\n", env_name, *variable_out);
}


void ski_input_env_load_str(char *env_name, char *variable){
    char *env_str;
    env_str = getenv(env_name);
    if(env_str && (strlen(env_str)>0)){
        assert(strlen(env_str) < 512);
        strcpy(variable, env_str);
    } else {
        // Env. variable not set or empty
        variable[0] = 0;
    }
	printf("[SKI] %s=%s\n", env_name, variable);
}


// Range should have the format [[@]@][+]<int>[-<int>](,<int>[-<int>])*
//   such as "2,4,5-7" or "4-6,9-12"
void ski_input_env_load_int_range(char *env_name, ski_input_int_range *variable){
    char *env_str;
	char *next_str;
    env_str = getenv(env_name);

	memset(variable, 0, sizeof(variable));

    if(env_str && (strlen(env_str)>0)){
		long long int i;
		int res;
		long long int d1;
		long long int d2;
		

		printf("[SKI] Parsing range: %s (env. variable %s)\n", env_str, env_name);
		if (env_str[0]=='%'){
			pair_mode = 1;
			env_str++;
		}

		if(env_str[0]=='@'){
			if(env_str[1]=='@'){
				// Double @@ means pairs of inputs
				variable->from_file_pairs = 1;
				env_str++;
				env_str++;
			}else{
				variable->from_file_single = 1;
				env_str++;
			}
		}

		if(env_str[0]=='+'){
			variable->relative = 1;
			env_str++;
		}

		while(1){
			d1 = strtoll(env_str, &next_str, 10);
			assert(env_str != next_str);
			printf("[SKI] d1 = %lld\n", d1);
			env_str = next_str;
			
			if (env_str[0] == '-'){
				// Range field (<d1>-<d2>)

				env_str++;

				d2 = strtoll(env_str, &next_str, 10);
				assert(env_str != next_str);
				printf("[SKI] d2 = %lld\n", d2);
				env_str = next_str;
				
				assert(d2>=d1);

				assert(variable->range_n + (d2-d1+1)< MAX_SKI_INTPUT_INT_RANGE);
				for(i=0;i<=(d2-d1);i++){
					variable->range[variable->range_n] = d1 + i;
					variable->range_n++;
				}

			}else if((env_str[0] == 0) || (env_str[0] == ',')){
				// Single value field (<d1>)
				assert(variable->range_n < MAX_SKI_INTPUT_INT_RANGE);
				variable->range[variable->range_n] = d1;
				variable->range_n++;
			}else{
				printf("[SKI] Unrecognizable value: %c\n", next_str[0]);
				assert(0);
			}

			if(env_str[0] == 0){
				return;
			}
			assert(env_str[0] == ',');
			env_str++;
		}
		
    } else {
        // Env. variable not set or empty
		printf("[SKI] ERROR: env. variable not defined (%s)\n", env_name);
		assert(0);
    }
}


// From http://nion.modprobe.de/blog/archives/357-Recursive-directory-creation.html
static void recursive_mkdir(const char *dir) {
	char tmp[256];
	char *p = NULL;
	size_t len;

	snprintf(tmp, sizeof(tmp),"%s",dir);
	len = strlen(tmp);

	if(tmp[len - 1] == '/'){
		tmp[len - 1] = 0;
	}

	for(p = tmp + 1; *p; p++){
		if(*p == '/'){
			*p = 0;
			mkdir(tmp, 0700);
			*p = '/';
		}
	}

	mkdir(tmp, 0700);
}

