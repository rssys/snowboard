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


#ifndef SKI_PRIORITIES_H
#define SKI_PRIORITIES_H



//XXX: possibly wrong name for the MACRO -- it's the maximum for the entire file?!? 
//TODO: add check to avoid overrun
#define SKI_MAX_CONFIG_PRIORITIES_PER_LINE 64*256
#define SKI_MAX_CONFIG_PRIORITIES_LENGHT 1024
#define SKI_MAX_CONFIG_PRIORITIES_FIELD_ENTRIES 50

typedef struct ski_config_priorities_entry {
    int cpu;
    int i;
    int n;
} ski_config_priorities_entry;


void ski_config_priorities_parse_file(CPUState *env, char *filename, int seed);

#endif
