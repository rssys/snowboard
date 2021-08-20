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


//#include <ski.h>
#include <ski-race-detector.h>


void ski_race_detector_init(ski_race_detector *rd){
    int i;

    memset(rd->last_access_per_cpu, 0, sizeof(rd->last_access_per_cpu));

    for(i=0; i<SKI_RD_MAX_CPU; i++){
        rd->last_access_per_cpu[i].cpu = i;
        rd->last_access_per_cpu[i].length = 0;
    }

    rd->races_n = 0;
    rd->total_races = 0;
}


void ski_race_detector_print(ski_race_detector *rd, char* trace_filename, int seed, int input1, int input2, FILE *fp_races){
    int i;
    for(i=0; i<rd->races_n; i++){
        ski_rd_race *r = &rd->races[i];

        fprintf(fp_races, "T: %s S: %d I1: %d I2: %d IP1: %x IP2: %x PMA1: %x PMA2: %x CPU1: %d CPU2: %d R1: %d R2: %d L1: %d L2: %d IC1: %d IC2: %d\n",
            trace_filename, seed, input1, input2,
            r->m1.ip_address, r->m2.ip_address,
            r->m1.physical_memory_address, r->m2.physical_memory_address,
            r->m1.cpu, r->m2.cpu,
            r->m1.is_read, r->m2.is_read,
            r->m1.length, r->m2.length,
            r->m1.instruction_count, r->m2.instruction_count);
    }

    printf("[SKI] ski_race_detector_print: %d/%d\n", rd->races_n, rd->total_races);
}





