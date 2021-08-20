#ifndef FORKALL_COOP_H
#define FORKALL_COOP_H

#ifndef __USE_GNU 
#define __USE_GNU
#endif // __USE_GNU

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // _GNU_SOURCE

#include <ucontext.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <unistd.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include <execinfo.h>
#include <limits.h>


#include <setjmp.h>
#include <string.h>

#include <sys/mman.h>
#include <stdarg.h>

#include <asm/ldt.h>
#include <asm/ptrace.h>

// XXX: Need to be really carefull to ensure the address is not in use 
// Libraries use addresses around and higher than 7f06a0b9a000, while very low values are used for the heap, globals, etc.

#define FORKALL_THREAD_STACK_BEGIN 0x5f06a0000000LL
//#define FORKALL_THREAD_STACK_BEGIN 0x2000000LL
#define FORKALL_THREAD_STACK_SIZE 0x20000LL
#define FORKALL_THREAD_STACK_DISTANCE (0x20000LL + 0x1000LL)

#define FORKALL_MAX_THREADS 10

#include <asm/prctl.h>
#include <sys/prctl.h>

// Disable to 
#define SKI_FORKALL_ENABLED


extern int ski_forkall_enabled;
extern int ski_forkall_round;

typedef struct struct_fork_thread {
    int tid_original;
	int tid_restore;

	pthread_t * thread_original_addr;

	pthread_t thread_original;
	pthread_t thread_restore;

    jmp_buf forkall_env;
    char stack[FORKALL_THREAD_STACK_SIZE];
    char tmp_stack[FORKALL_THREAD_STACK_SIZE];
    int thread_seq;
    void *stack_min;
    void *stack_max;
} forkall_thread;


pid_t ski_gettid(void);

void ski_forkall_initialize(void);
pid_t ski_forkall_master(void(*wake_cb)(void));
void ski_forkall_slave(int *did_fork, int *is_child);
void ski_forkall_thread_add_self_tid(void);

int ski_forkall_pthread_kill(pthread_t thread, int sig);
int ski_forkall_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
int ski_forkall_timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1);

extern int ski_init_options_forkall_rounds;
extern int ski_init_options_forkall_concurrency;

extern int ski_forkall_parent_simulate_child;

#endif
