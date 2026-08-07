#ifndef ROBU_LIBC_SYS_TIME_H
#define ROBU_LIBC_SYS_TIME_H
#include <sys/types.h>
#include <time.h>
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
struct itimerval {
    struct timeval it_interval;
    struct timeval it_value;
};
int gettimeofday(struct timeval *tv, void *tz);
int settimeofday(const struct timeval *tv, const void *tz);
int utimes(const char *path, const struct timeval times[2]);
#endif
