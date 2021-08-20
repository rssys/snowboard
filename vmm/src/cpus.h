#ifndef QEMU_CPUS_H
#define QEMU_CPUS_H

/* cpus.c */
void qemu_init_cpu_loop(void);
void resume_all_vcpus(void);
void pause_all_vcpus(void);
void cpu_stop_current(void);

void cpu_synchronize_all_states(void);
void cpu_synchronize_all_post_reset(void);
void cpu_synchronize_all_post_init(void);

void ski_enable_clock(void);
void ski_disable_clock(void);

/* vl.c */
extern int smp_cores;
extern int smp_threads;
void set_numa_modes(void);
void set_cpu_log(const char *optarg);
void set_cpu_log_filename(const char *optarg);
void list_cpus(FILE *f, fprintf_function cpu_fprintf, const char *optarg);


void ski_forkall_reinit_qemu_init_cpu_loop(void);
void ski_forkall_wait_firstcpu(void);
void ski_forkall_release_locks(void);
void ski_forkall_aquire_locks(void);

#endif
