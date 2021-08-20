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


#ifndef SKI_INPUT_H
#define SKI_INPUT_H


#define MAX_SKI_INTPUT_INT_RANGE (1024*1024)
#define SKI_INPUT_FILE_MAX (256*1024)
#define SKI_INPUT_PAIRS_FILE_MAX (5*1024*1024)

long long int ski_input_init(void);
void ski_input_fork_init(int *out_new_input);
int ski_input_fork_skip_init();

extern char ski_init_execution_ts[256];


#endif
