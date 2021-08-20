

#include "forkall-coop.h"

#include <asm/prctl.h>
#include <sys/prctl.h>


// Testing variables
//static struct user_desc dummy;
//static struct pt_regs dummy2;

static forkall_thread forkall_threads[FORKALL_MAX_THREADS];
static int thread_count = 0;

static pthread_mutex_t forkall_mutex = PTHREAD_MUTEX_INITIALIZER;
static int forkall_forking = 0;
static int forkall_threads_done = 0;
static int forkall_nthreads = -1;

static int global_thread_seq = 0;

int ski_forkall_enabled = 0;
int ski_forkall_round = 0;


#pragma GCC push_options
#pragma GCC optimize 0
#pragma GCC optimization_level 0
//#pragma GCC optimize ("O0")

static int ski_create_thread_custom_stack(forkall_thread *t, pthread_t *thread, const pthread_attr_t *attr_arg, void *(*start_routine) (void *), void *arg);

/* 
	Semantic differences between this implementation of fork and the common single-threaded 
	fork(). Some of the differences are that forkall_slave():
  	  	a) not always forks (only if there is a signal from the master), 
		b) returns info through the arguments, 
		c) hangs the parent threads (we don't need them to make progress)
		d) subsequent forkall_master() fork the process address space but restore the secondary 
			threads as they were during the first forkall_master()
*/

// Uncomment to enable debugging
//#define FORKALL_DEBUGGING

pid_t ski_gettid(void){
    pid_t own_tid = syscall(SYS_gettid);
    return own_tid;
}


int ski_forkall_timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1)
{
    long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
    result->tv_sec = diff / 1000000;
    result->tv_usec = diff % 1000000;

    return (diff<0);
}



void ski_log_forkall(const char* format, ...)
	__attribute__ ((format (printf, 1, 2)));

void ski_log_forkall(const char* format, ...)
{ 

#ifdef FORKALL_DEBUGGING
#ifndef SKI_DISABLE_FORKALL_OUTPUT
	char buff[512];
	struct timeval tv;
	gettimeofday(&tv, NULL);

    va_list argptr;
    va_start(argptr, format);
    vsprintf(buff, format, argptr);
	fprintf(stdout, "[%ld.%06ld] [%04d %04d] %s", tv.tv_sec, tv.tv_usec, (int) getpid(), (int) ski_gettid(), buff);
	fflush(0);
    va_end(argptr);
#endif
#endif
}

int arch_prctl(int code, unsigned long *addr);


/* This seems that it would also work to relocate the TLS (by changing the TS in Linux) */
static void ski_get_fsgs_addr(void){
	int code = ARCH_GET_FS;
	unsigned long  value;
	unsigned long *addr =  &value;
	int res = arch_prctl(code, addr);
	ski_log_forkall("FS = %lx\n", value);
	code = ARCH_GET_GS;
	res = arch_prctl(code, addr);
	ski_log_forkall("GS = %lx\n", value);
}


// This could also be encapsulated in the custom pthread_create function
/*
void forkall_thread_add_self(void){
	int dummy;
	int tid = ski_gettid();

	ski_log_forkall("Adding thread tid %d\n", tid);
	
	pthread_mutex_lock(&forkall_mutex);
	forkall_thread* t = &forkall_threads[thread_count]; 
	thread_count++;
	assert(thread_count<FORKALL_MAX_THREADS);
	printf("Using the structure %d at %p (with %d bytes)\n", thread_count, t, (int) sizeof(forkall_thread));
	pthread_mutex_unlock(&forkall_mutex);

	memset(t, 0, sizeof(forkall_thread));
	t->tid = tid;
	t->stack_seq = -1;
	int i;
	for(i=0;i<FORKALL_MAX_THREADS;i++){
		void* stack_min = (void*)(FORKALL_THREAD_STACK_BEGIN + (i * FORKALL_THREAD_STACK_DISTANCE));
		void* stack_max = stack_min + FORKALL_THREAD_STACK_SIZE; 
		if((((void*)&dummy) > stack_min) && (((void*)&dummy) < stack_max)){
			t->stack_min = stack_min;
			t->stack_max = stack_max;
			t->stack_seq = i;
			ski_log_forkall("Found stack: %p - %p (seq = %d)\n", stack_min, stack_max, i);
			break;
		}
	}
	assert(t->stack_seq >= 0);
}
*/

void ski_forkall_thread_add_self_tid(void){
    int dummy;
    int tid = ski_gettid();
    int i;

    ski_log_forkall("Adding thread tid %d\n", tid);

    pthread_mutex_lock(&forkall_mutex);

    // Try to find self thread by looking at the stack addresses
    for(i=0;i<thread_count;i++){
        forkall_thread* t = &forkall_threads[i];

        if((((void*)&dummy) > t->stack_min) && (((void*)&dummy) < t->stack_max)){
            t->tid_original = tid;
            ski_log_forkall("Found stack: %llx - %llx (seq = %d, tid = %d)\n", t->stack_min, t->stack_max, i, tid);
            pthread_mutex_unlock(&forkall_mutex);
            return;
        }
    }
    assert(0);
    pthread_mutex_unlock(&forkall_mutex);
}


forkall_thread * ski_forkall_thread_new(void){
    // Find free slot   
    pthread_mutex_lock(&forkall_mutex);
    int thread_seq = thread_count;
    thread_count++;
    assert(thread_count<FORKALL_MAX_THREADS);
    pthread_mutex_unlock(&forkall_mutex);

    ski_log_forkall("Adding new thread_seq %d\n", thread_seq);

    forkall_thread* t = &forkall_threads[thread_seq];

    // Clear the struture
    memset(t, 0, sizeof(forkall_thread));

    // Initialize the basic fields
    t->thread_seq = thread_seq;
    t->stack_min = (void*)(FORKALL_THREAD_STACK_BEGIN + (thread_seq * FORKALL_THREAD_STACK_DISTANCE));
    t->stack_max = t->stack_min + FORKALL_THREAD_STACK_SIZE;
    t->tid_original = -1;
    t->tid_restore = -1;

    ski_log_forkall("Added new thread_seq %d (stack_min: %p, stack_max: %p)\n", thread_seq, t->stack_min, t->stack_max);

    return t;
}


static forkall_thread* ski_thread_find(int tid){
	int i;
	ski_log_forkall("Finding thread tid %d\n", tid);
	for(i=0;i<thread_count;i++){
		forkall_thread* t = &forkall_threads[i]; 
		if(t->tid_original == tid){
			return t;
		}
	}
	return 0;
}

static int ski_tgkill(int tgid, int tid, int sig){
    int err;
	ski_log_forkall("Sending signal (tgid = %d, tid = %d, sig = %d)\n", tgid, tid, sig);
    int res = syscall(SYS_tkill, tid, sig);
    err = errno;
	if(res){
		ski_log_forkall("tgkill: errno = %d\n", err);
		ski_log_forkall("Unable to send the signal: error = %s\n", strerror(err));
	}
	return res;
}

static int ski_forkall_is_child = 0;


int ski_forkall_pthread_kill(pthread_t thread, int sig){
	int i;

	if(!ski_forkall_is_child){
		return pthread_kill(thread, sig);
	}
	
	for(i=0;i<thread_count;i++){
		forkall_thread* t = &forkall_threads[i]; 
		if(memcmp(&t->thread_original, &thread, sizeof(pthread_t)) == 0){
			// Found the destination thread
			ski_log_forkall("Found the destination thread (seq = %d, tid_restore = %d)\n", t->thread_seq, t->tid_restore);
			assert(t->tid_restore != 0);
			int res = ski_tgkill(-1, t->tid_restore, sig);	
			return res;
		}
	}

	ski_log_forkall("Unable to find the destination thread\n");
	assert(0);
	return 0;
	
}

int ski_forkall_parent_simulate_child = 0;

void ski_forkall_slave(int *did_fork, int *is_child){
	if((!forkall_forking) || ski_forkall_parent_simulate_child){
		*did_fork = 0;
		return;
	}

	ski_log_forkall("forkall_slave(): Preparing for next forkall (forkall_slave)\n");

	int tid = ski_gettid();
	forkall_thread *t = ski_thread_find(tid);
	assert(t); // If this fails it's probably because the thread main did not call the forkall_thread_add_self() function forkall_thread_add_self(); 
	memcpy(t->stack, t->stack_min, FORKALL_THREAD_STACK_SIZE);
	int restoring = setjmp(t->forkall_env);

	if(!forkall_forking || restoring){
		ski_log_forkall("forkall_slave(): Restoring (forkall_forking: %d, restoring: %d)\n", forkall_forking, restoring);
		*did_fork = 1;
		*is_child = 1;
		return;
	}

	pthread_mutex_lock(&forkall_mutex);
	forkall_threads_done += 1;
	pthread_mutex_unlock(&forkall_mutex);

	*did_fork = 1;
	*is_child = 0;
	
	// XXX: This implementation always hangs the secondary threads of the parent process that issued the fork

	ski_log_forkall("[SECUNDARY] Done saving own state. Hanging.\n");
	while(1){
		sleep(1);
		if(ski_forkall_parent_simulate_child){
			// Debugging: used by SKI_DEBUG_PARENT_EXECUTES_ENABLED
			*did_fork = 1;
			*is_child = 1;
			return;
		}
	}

	// TODO: Get out of the barrier
}
void ski_forkall_thread_restore_registers(forkall_thread * t) __attribute__((optimize(0)));

void ski_forkall_thread_restore_registers(forkall_thread * t) {

	longjmp(t->forkall_env, 1);
	ski_log_forkall("SHOULD NOT HAVE REACHED HERE\n");
	
}

void* ski_forkall_thread_restore(void *param) __attribute__((optimize(0)));

void* ski_forkall_thread_restore(void *param){
	forkall_thread *t = (forkall_thread*) param;
	int dummy_stack;
	char *tmp_stack_addr = &t->tmp_stack[4024];
	char *stack = t->stack;
	char *stack_min = t->stack_min;

	t->tid_restore = ski_gettid();

	ski_get_fsgs_addr();

	ski_log_forkall("Thread restoring the thread tid: %d (dummy_stack: %p)\n", t->tid_original, &dummy_stack);

	asm("mov %0, %%esp;"  /* Use temporary stack */

	    "push %1;"        /* Push the parameter that is going to be used because of the env on to the temporary stack */

		"mov    %2,%%rdi;"
		"mov    %3,%%rsi;"
	    "mov    %4,%%rdx;"
		"callq  %P5;"     /* Call memcpy to restore the original stack (from the backup up stack containing the TLS)*/
		/* memcpy(stack_min, stack, FORKALL_THREAD_STACK_SIZE); */


		"pop %%rdi;"
		"callq  %P6;"     /* Call forkall_thread_restore2() which does the actual restore of the registers (including stack pointer) */
		/* forkall_thread_restore2((forkall_thread*) param); */
			: 
			: "m" (tmp_stack_addr), "m" (param), "m" (stack_min), "m" (stack), "i" (FORKALL_THREAD_STACK_SIZE), "i" (memcpy), "i" (ski_forkall_thread_restore_registers)
			: "%esp");	  /* No need to clobber becasuse we don't return */


	// DOES NOT RETURN


	ski_log_forkall("SHOULD NOT HAVE REACHED HERE\n");
	assert(0);

}


void ski_forkall_patch_thread_references(void){
	int i;
	for(i=0;i<thread_count;i++){
		int res;
		forkall_thread *t = &forkall_threads[i];
		ski_log_forkall("Trying to patch the thread structure for the thread tid_original: %d\n", t->tid_original);
		memcpy(t->thread_original_addr, &t->thread_restore, sizeof(t->thread_restore));
	}

}

pid_t ski_forkall_master(void(*wake_cb)(void)){
	ski_log_forkall("[MASTER] Requesting forkall (forkall_master())\n");

	pthread_mutex_lock(&forkall_mutex);
	forkall_forking = 1;
	pthread_mutex_unlock(&forkall_mutex);

	while(1){
		int threads_done;
		int nthreads;

		pthread_mutex_lock(&forkall_mutex);
		threads_done = forkall_threads_done;
		nthreads = thread_count;
		pthread_mutex_unlock(&forkall_mutex);

		if(threads_done == nthreads){
			ski_log_forkall("[MASTER] Finished waiting for all threads to be ready (%d/%d)\n", threads_done, nthreads);
			break;
		}

		ski_log_forkall("[MASTER] Waiting for all threads to be ready (%d/%d)\n", threads_done, nthreads);
		
		//XXX: QEMU Specific!! Tries to wake up the first cpu when it's waiting for the initial kick (cpus.c)
		(*wake_cb)();

		ski_log_forkall("[MASTER] Waiting for all threads to be ready (%d/%d)\n", threads_done, nthreads);
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 500000;
		nanosleep(&ts, NULL);
	}

	// Now that we got the state of all threads 

	int pid = -1;
	while(pid != 0)
	if((pid = fork())){
		// Parent
		ski_log_forkall("Parent (child pid = %d)\n", pid);
		return pid;
	}else{
		// Child process
		// We can now recreate all the threads (also need to release them)
		ski_log_forkall("Child\n");

		// Prevent child secondary threads from thinking they have been flaged to do another fork
		forkall_forking = 0;

		// Set a global variable to make sure that pthread_kill,... are able to distinguish
		ski_forkall_is_child = 1;

		// Need to ensure we don't use the same stack as the previous threads were using, otherwise we would end up overriding their values
		int i;
		for(i=0;i<thread_count;i++){
			int res;
			forkall_thread *t = &forkall_threads[i];
			ski_log_forkall("Trying to restore thread tid_original: %d\n", t->tid_original);
			if((res=ski_create_thread_custom_stack(t, &t->thread_restore, 0, &ski_forkall_thread_restore, t))){
				ski_log_forkall("ERROR: Unable to create thread!!\n");
				assert(0);
			}

			//sleep(2);
			/*
			ski_log_forkall("Sending signal to thread seq: %d\n", i);
	    	int err = forkall_pthread_kill(t->thread_original, (SIGRTMIN+4));
	   	 	if (err) {
    	    	ski_log_forkall("forkall:%s: %s unable to send the IPI in this weird way (err = %d)...\n", __func__, strerror(err), err);
		    }
			*/
		}

		// XXX: This also has a race with the secondary threads created...
		//forkall_patch_thread_references();

		ski_log_forkall("[MASTER] restored\n");
		return pid;
	}

	assert(0);
	return -1;
}



void ski_forkall_initialize(void){
	//forkall_nthreads = nthreads;
	forkall_forking = 0;
}



int ski_forkall_pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg){
	ski_log_forkall("forkall_pthread_create\n");

	/*
	pthread_mutex_lock(&forkall_mutex);
	thread_seq = global_thread_seq;
	global_thread_seq++;
    pthread_mutex_unlock(&forkall_mutex);
	*/

	forkall_thread *t = ski_forkall_thread_new();
	t->thread_original_addr = thread;
	int ret = ski_create_thread_custom_stack(t, thread, attr, start_routine, arg);
	memcpy(&t->thread_original, thread, sizeof(pthread_t));

	return ret;

}

// Based on: http://www.cs.cf.ac.uk/Dave/C/node30.html
static int ski_create_thread_custom_stack(forkall_thread *t, pthread_t *thread, const pthread_attr_t *attr_arg, void *(*start_routine) (void *), void *arg){
	pthread_attr_t *attr;
	pthread_attr_t attr_local;
	int ret;
	void *stackbase = (void*)(FORKALL_THREAD_STACK_BEGIN + (t->thread_seq * FORKALL_THREAD_STACK_DISTANCE));
	int stacksize = FORKALL_THREAD_STACK_SIZE;

	ski_log_forkall("Creating thread seq %d with stack at addresss: %p (thread: %p attr_arg: %p start_routine: %p arg: %p)\n", t->thread_seq, stackbase, thread, attr_arg, start_routine, arg);
	stackbase = mmap (stackbase, stacksize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (stackbase == MAP_FAILED)
	{
		perror ("mmap failed");
		assert(0);
		return 1;
	}

	// XXX: We should save the passed attr and restore them when recreating the threads...

	attr_arg = 0;

	/* initialized with default attributes */
	if(attr_arg == 0){
		attr = &attr_local;
		ret = pthread_attr_init(attr);
		assert(ret == 0);
	}else{
		attr = attr_arg;
	}
	/* setting the base address in the attribute */
	ret = pthread_attr_setstack(attr, stackbase, stacksize);
	assert(ret == 0);

	ski_log_forkall("Launching new thread\n");
	/* address and size specified */
	ret = pthread_create(thread, attr, start_routine, arg);
	assert(ret == 0);
	
	if(attr_arg == 0){
		pthread_attr_destroy(attr);
	}
	return ret;
}



#pragma GCC pop_options

