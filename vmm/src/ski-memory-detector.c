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


#include "ski-memory-detector.h"

#ifdef SKI_MEMORY_DETECTOR_ENABLED

void ski_memory_detector_init(ski_md *md){
    memset(md, 0, sizeof(ski_md));
}


void ski_memory_detector_reset(ski_md *md){

	// Reset the ip count values, but keep the ip values and the hash structure
	/*for(i=0;i<md->instructions_n;i++){
		md->instructions[i].count = 0;
	}*/
	printf("ski_memory_detector_reset()\n");
	md->instructions_hash = 0;
	md->instructions_n = 0;
	md->total_instructions = 0;
	md->total_distinct_instructions = 0;
}
                                            


void ski_memory_detector_print(ski_md *md, char* trace_filename, int seed, int input1, int input2, FILE *fp_md){
    int i;
    char mem_hits[SKI_MEMORY_DETECTOR_HITS_MAX];

    printf("ski_memory_detector_print: %d/%d\n", md->total_instructions, md->total_distinct_instructions);
	
	if(md->total_instructions==0)
		return;

    for(i=0; i<md->instructions_n; i++){
		int j;
        ski_md_entry *mde = &md->instructions[i];
    	char *mem_hits_ptr = mem_hits;
    	mem_hits[0] = 0;

		for(j=0;j<mde->memory_addresses_n;j++){
            mem_hits_ptr += sprintf(mem_hits_ptr, "%x (%s%s %d) ", 
				mde->memory_addresses[j].memory_address, 
				(mde->memory_addresses[j].type & SKI_MEMORY_DETECTOR_MA_READ) ? "R" : "", (mde->memory_addresses[j].type & SKI_MEMORY_DETECTOR_MA_WRITE) ? "W" : "", 
				mde->memory_addresses[j].size_bytes);
        }

		fprintf(fp_md, "%d %d %d %d %x %s\n",
			seed, input1, input2,
			mde->count,
			mde->eip_address,
			mem_hits);
		
/*		fprintf(fp_md, "T: %s S: %d I1: %d I2: %d TI: %d TDI: %d IP: %x HITS: %s\n",
			trace_filename, seed, input1, input2,
			md->total_instructions, md->total_distinct_instructions,
			mde->eip_address,
			mem_hits);
*/
	}
}




#endif // SKI_MEMORY_DETECTOR_ENABLED

