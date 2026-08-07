#ifndef ROBU_LIBC_SCHED_H
#define ROBU_LIBC_SCHED_H
int sched_yield(void);
int sched_getaffinity(int pid, unsigned long cpusetsize, void *mask);
int sched_setaffinity(int pid, unsigned long cpusetsize, const void *mask);
#endif
