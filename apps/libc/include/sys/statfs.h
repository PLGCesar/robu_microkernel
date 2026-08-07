#ifndef ROBU_LIBC_SYS_STATFS_H
#define ROBU_LIBC_SYS_STATFS_H
#include <sys/types.h>
struct statfs {
    long f_type;
    long f_bsize;
    long f_frsize;
    long f_blocks;
    long f_bfree;
    long f_bavail;
    long f_files;
    long f_ffree;
    long f_namelen;
};
int statfs(const char *path, struct statfs *buf);
int fstatfs(int fd, struct statfs *buf);
#endif
