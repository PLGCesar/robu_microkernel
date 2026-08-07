#ifndef ROBU_LIBC_TIME_H
#define ROBU_LIBC_TIME_H
#include <sys/types.h>
#ifndef NULL
#define NULL ((void *)0)
#endif
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
time_t time(time_t *tloc);
struct tm *localtime(const time_t *timep);
struct tm *gmtime(const time_t *timep);
time_t mktime(struct tm *tm);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
double difftime(time_t a, time_t b);
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(int clk_id, struct timespec *tp);
struct tm *localtime_r(const time_t *timep, struct tm *result);
struct tm *gmtime_r(const time_t *timep, struct tm *result);
char *strptime(const char *s, const char *format, struct tm *tm);
void tzset(void);
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#endif
