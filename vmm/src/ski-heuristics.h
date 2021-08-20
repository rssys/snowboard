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


#ifndef SKI_HEURISTICS

int ski_threads_all_wake_rs(CPUState *env, int mem_address);
void ski_threads_self_wait_rs(CPUState *env, int is_pause);
void ski_threads_self_wait_hlt(CPUState *env);


#endif
