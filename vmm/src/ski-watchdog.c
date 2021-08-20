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


#include <stdio.h>
#include <stdlib.h>

#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <signal.h>
#include <time.h>

#include "ski-debug.h"


void ski_print_ts(){
    struct timeval start;
	gettimeofday(&start, NULL);
	long int sec = start.tv_sec;
	long int usec = start.tv_usec;

	printf("[SKI] Current time: %ld.%06ld\n", start.tv_sec, start.tv_usec);
}

int ski_watchdog_fired = 0;

void ski_watchdog_check(void)
{
	if(ski_watchdog_fired){
		printf ("[SKI] [WATCHDOG] Watchdog expired! Killing child process.\n");
		ski_watchdog_fired = 1;
		SKI_ASSERT_MSG(0, SKI_EXIT_WATCHDOG, "WATCHDOG");
	}
}

// There were some functions that could cause reentrancy problem when called from the signal handler
void ski_timer_handler(union sigval arg)
{
	static int count = 0;
	ski_watchdog_fired = 1;
	//printf ("[SKI] timer expired %d times\n", ++count);
	//printf ("[SKI] [WATCHDOG] Watchdog expired! Killing child process.\n");
	//ski_print_ts();
	//SKI_ASSERT_MSG_SIGNAL_SAFE(0, SKI_EXIT_WATCHDOG, "WATCHDOG");
	  // -> This assert is not safe because it invokes the stats function to compute the communication points...
}

void ski_watchdog_init(int seconds)
{
	timer_t timerid;
	struct sigevent sev;
	struct itimerspec its;
	struct sigaction sa;

	printf("[SKI] [WATCHDOG] Timer set to %d seconds\n", seconds);
	ski_print_ts();

	// Install timer_handler as the signal handler for SIGVTALRM. 
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &ski_timer_handler;
	assert(sigaction (SIGPROF, &sa, NULL) == 0);

	/* Create the timer */

	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGPROF;
	sev.sigev_value.sival_ptr = &timerid;
	sev.sigev_notify_function = 0;
	
	//if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1){
	if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1){
	   perror("timer_create");
	}

	//printf("timer ID is 0x%lx\n", (long) timerid);

	/* Start the timer */

	its.it_value.tv_sec = seconds;
	its.it_value.tv_nsec = 0;
	its.it_interval.tv_sec = seconds;
	its.it_interval.tv_nsec = 0;

	if (timer_settime(timerid, 0, &its, NULL) == -1)
		perror("timer_settime");

	/* Sleep for a while; meanwhile, the timer may expire
	  multiple times */

}

/*
void ski_watchdog_init(int seconds)
{
	// SIGEV_THREAD

	printf("[SKI] [WATCHDOG] Timer set to %d seconds\n", seconds);
	ski_print_ts();

	struct sigaction sa;
	struct itimerval timer;

	// Install timer_handler as the signal handler for SIGVTALRM. 
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &ski_timer_handler;
	//sigaction (SIGVTALRM, &sa, NULL);
	assert(sigaction (SIGALRM, &sa, NULL) == 0);

	// Configure the timer to expire after 250 msec... 
	timer.it_value.tv_sec = seconds;
	timer.it_value.tv_usec = 0;

	// ... and every 250 msec after that. 
	timer.it_interval.tv_sec = seconds;
	timer.it_interval.tv_usec = 0;

	// Start a virtual timer. It counts down whenever this process is executing.

	// ITIMER_VIRTUAL counts both threads together...
	//setitimer (ITIMER_VIRTUAL, &timer, NULL);
	
	assert(setitimer (ITIMER_PROF, &timer, NULL) == 0);

	printf("Test: Sleeping for 20 seconds\n");
	sleep(20);
}

*/
