#ifndef ROBU_LIBC_SYS_XATTR_H
#define ROBU_LIBC_SYS_XATTR_H
#include <sys/types.h>
ssize_t getxattr(const char *path, const char *name, void *value, size_t size);
ssize_t lgetxattr(const char *path, const char *name, void *value, size_t size);
ssize_t fgetxattr(int fd, const char *name, void *value, size_t size);
ssize_t listxattr(const char *path, char *list, size_t size);
ssize_t llistxattr(const char *path, char *list, size_t size);
ssize_t flistxattr(int fd, char *list, size_t size);
int setxattr(const char *path, const char *name, const void *value, size_t size, int flags);
int lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags);
int fsetxattr(int fd, const char *name, const void *value, size_t size, int flags);
int removexattr(const char *path, const char *name);
ssize_t xattr_get(const char *path, const char *name, void *value, size_t size);
ssize_t xattr_lget(const char *path, const char *name, void *value, size_t size);
ssize_t xattr_fget(int fd, const char *name, void *value, size_t size);
ssize_t xattr_list(const char *path, char *list, size_t size);
ssize_t xattr_llist(const char *path, char *list, size_t size);
ssize_t xattr_flist(int fd, char *list, size_t size);
ssize_t xattr_set(const char *path, const char *name, const void *value, size_t size, int flags);
ssize_t xattr_lset(const char *path, const char *name, const void *value, size_t size, int flags);
ssize_t xattr_fset(int fd, const char *name, const void *value, size_t size, int flags);
#endif
