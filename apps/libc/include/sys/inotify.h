#ifndef ROBU_LIBC_SYS_INOTIFY_H
#define ROBU_LIBC_SYS_INOTIFY_H
#include <sys/types.h>
#define IN_MODIFY 0x00000002
#define IN_CREATE 0x00000100
#define IN_DELETE 0x00000200
struct inotify_event {
    int wd;
    unsigned int mask;
    unsigned int cookie;
    unsigned int len;
    char name[];
};
int inotify_init(void);
int inotify_add_watch(int fd, const char *pathname, unsigned int mask);
int inotify_rm_watch(int fd, int wd);
#endif
