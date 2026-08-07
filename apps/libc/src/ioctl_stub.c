#include <sys/ioctl.h>
#include <errno.h>
int ioctl(int fd, unsigned long request, ...) {
    (void)fd;
    (void)request;
    errno = ENOTTY;
    return -1;
}
