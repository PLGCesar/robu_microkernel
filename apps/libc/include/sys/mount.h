#ifndef ROBU_LIBC_SYS_MOUNT_H
#define ROBU_LIBC_SYS_MOUNT_H
#include <sys/statfs.h>
#include <sys/xattr.h>
#define MNT_WAIT 1
int mount(const char *source, const char *target, const char *fstype,
         unsigned long flags, const void *data);
int umount(const char *target);
int umount2(const char *target, int flags);
#endif
