#ifndef ROBU_LIBC_UTIME_H
#define ROBU_LIBC_UTIME_H
#include <sys/types.h>
struct utimbuf {
    time_t actime;
    time_t modtime;
};
int utime(const char *path, const struct utimbuf *times);
#endif
