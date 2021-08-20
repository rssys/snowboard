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


#ifndef SKI_HEURISTICS_STATISTICS
#define SKI_HEURISTICS_STATISTICS



#define SKI_HEURISTICS_STATISTICS_HLT 1
#define SKI_HEURISTICS_STATISTICS_PAUSE 2
#define SKI_HEURISTICS_STATISTICS_LOOP 3
#define SKI_HEURISTICS_STATISTICS_STARVATION 4

void ski_heuristics_statistics_print(int heuristic_type, int instruction_count, int cpu_no, int instruction_address);

extern int ski_init_options_heuristics_statistics_enabled;

#endif
