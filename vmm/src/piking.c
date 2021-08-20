
//#include "targphys.h"


// From cpu-exec.c
//#include "config.h"
//#include "cpu.h"
//#include "disas.h"
//#include "tcg.h"
//#include "qemu-barrier.h"

// From balloon.c
#include "monitor.h"
#include "cpu-common.h"
#include "kvm.h"
//#include "balloon.h"
#include "trace.h"
#include "qapi-types.h"
#include "qmp-commands.h"

#include "ski-debug.h"

void pike_resume_all_cpus(void);
void pike_cpu(CPUState *env);
int piking_select_cpu(int cpu_index);

void pike_resume_all_cpus(void)
{
	// Similar to cpus.c:resume_all_vcpus but we don't need to change the time settings
    CPUState *penv = first_cpu;

    while (penv) {
        penv->stop = 0;
        penv->stopped = 0;
        qemu_cpu_kick(penv);
        penv = (CPUState *)penv->next_cpu;
    }
}

void pike_cpu(CPUState *env)
{
    CPUState *penv = first_cpu;
	int i = 0;

    //qemu_clock_enable(vm_clock, false);
    while (penv) {
		if(penv==env){
			penv->stop = 0;
			penv->stopped = 0;
			printf("CPU %d: Running\n",i);
		}else{
			printf("CPU %d: Suspending\n",i);
			penv->stop = 1;
		}
        qemu_cpu_kick(penv);
        penv = (CPUState *)penv->next_cpu;
		i++;
    }
}

/*
void pause_all_vcpus(void)
{
    CPUState *penv = first_cpu;

    qemu_clock_enable(vm_clock, false);
    while (penv) {
        penv->stop = 1;
        qemu_cpu_kick(penv);
        penv = (CPUState *)penv->next_cpu;
    }

    while (!all_vcpus_paused()) {
        qemu_cond_wait(&qemu_pause_cond, &qemu_global_mutex);
        penv = first_cpu;
        while (penv) {
            qemu_cpu_kick(penv);
            penv = (CPUState *)penv->next_cpu;
        }
    }
}

void resume_all_vcpus(void)
{
    CPUState *penv = first_cpu;

    qemu_clock_enable(vm_clock, true);
    while (penv) {
        penv->stop = 0;
        penv->stopped = 0;
        qemu_cpu_kick(penv);
        penv = (CPUState *)penv->next_cpu;
    }
}

*/

int piking_select_cpu(int cpu_index)
{
    CPUState *env;
	int i;

	if(cpu_index != -1) 
	{	
		bool cpu_found = false;
    	for (i = 0 , env = first_cpu; env != NULL; env = env->next_cpu, i++) {
			printf("i = %d\n", i);
			if(i == cpu_index){
				cpu_found = true;
				printf("cpu_index found\n");
				break;
			}
	    }
		if(cpu_found == false){
			return -1;			
		}else{
			pike_cpu(env);
			return 0;
		}
	}
	pike_resume_all_cpus();
	return 0;
}

int do_piking_select_cpu(Monitor *mon, const QDict *params,
                              QObject **ret_data)
{
    CPUState *env;
    int cpu_index = qdict_get_try_int(params, "cpu-index", -1);
	int i;
	printf("do_piking_select_cpu: Finding cpu with index = %d\n", cpu_index);	
	if(cpu_index != -1) 
	{	
		bool cpu_found = false;
		printf("iterating over the cpus\n");	
    	for (i = 0 , env = first_cpu; env != NULL; env = env->next_cpu, i++) {
			printf("i = %d\n", i);
			if(i == cpu_index){
				cpu_found = true;
				printf("cpu_index found\n");
				break;
			}
	    }

		if(cpu_found == false){
			
        	qerror_report(QERR_INVALID_PARAMETER, "cpu-index not found\n");
			return -1;			
		}
	}

	return piking_select_cpu(cpu_index);
}


int ski_reset(void)
{
    CPUState *env;
	int i;
	printf("ski_reset: Resetting ski for all cpus\n");
	ski_exec_trace_print_comment("User requested reset");
	ski_exec_trace_stop(first_cpu);
	for (i = 0 , env = first_cpu; env != NULL; env = env->next_cpu, i++) {
		SKI_TRACE("............................ RESET .............................\n");
		printf("i = %d\n", i);
		//XXX: Maybe we should ensure that we make these values go to their actual state before the test (which could be different than running...)
		if(env->ski_active == true){
			printf("Resuming CPU %d (testing CPU)\n", i);
			env->stopped = false;
			env->stop = false;
		}else if(ski_block_other_cpus){
			// Presume that non-testing CPUs were running before the test
			printf("Resuming CPU %d (non-testing CPU)\n",i);
			env->stopped = false;
			env->stop = false;
		}
		env->ski_active = false;
		tb_flush(env);
	}
	ski_reset_common();
	return 0;
}

int do_ski_reset(Monitor *mon, const QDict *params,
							QObject **ret_data)
{
	return ski_reset();
}

/*


    int ret = 0;
    Monitor *old_mon, hmp;
    CharDriverState mchar;

    memset(&hmp, 0, sizeof(hmp));
    qemu_chr_init_mem(&mchar);
    hmp.chr = &mchar;

    old_mon = cur_mon;
    cur_mon = &hmp;

    if (qdict_haskey(params, "cpu-index")) {
        ret = monitor_set_cpu(qdict_get_int(params, "cpu-index"));
        if (ret < 0) {
            cur_mon = old_mon;
            qerror_report(QERR_INVALID_PARAMETER_VALUE, "cpu-index", "a CPU number");
            goto out;
        }
    }

    handle_user_command(&hmp, qdict_get_str(params, "command-line"));

    cur_mon = old_mon;

    if (qemu_chr_mem_osize(hmp.chr) > 0) {
        *ret_data = QOBJECT(qemu_chr_mem_to_qs(hmp.chr));
    }

out:
    qemu_chr_close_mem(hmp.chr);
    return ret;
}

*/
