#include <sys/inotify.h>
#include <sys/syscall.h>
#include <errno.h>
int inotify_init(void) {
    errno = ENOSYS;
    return -1;
}
int inotify_add_watch(int fd, const char *pathname, unsigned int mask) {
    (void)fd;
    (void)pathname;
    (void)mask;
    errno = ENOSYS;
    return -1;
}
int inotify_rm_watch(int fd, int wd) {
    (void)fd;
    (void)wd;
    errno = ENOSYS;
    return -1;
}
long syscall(long number, ...) {
    (void)number;
    errno = ENOSYS;
    return -1;
}
