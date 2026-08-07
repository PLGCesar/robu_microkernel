#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "robu/types.h"
#include "robu/ramfs.h"
#include "libc_internal.h"
struct DIR {
    uint64_t cursor;
    uint64_t dir_ino;
    int fd;
};
DIR *opendir(const char *path) {
    char resolved[64];
    __libc_resolve_path(path, resolved, sizeof(resolved));
    uint64_t dir_ino;
    if (strcmp(resolved, "/") == 0) {
        dir_ino = RAMFS_ROOT_INO;
    } else {
        char name[RAMFS_PATH_MAX];
        uint64_t size, ino;
        int is_dir;
        const char *p = resolved[0] == '/' ? resolved + 1 : resolved;
        strlcpy(name, p, sizeof(name));
        if (ramfs_stat(name, &size, &is_dir, &ino) != 0 || !is_dir) {
            errno = ENOTDIR;
            return 0;
        }
        dir_ino = ino;
    }
    DIR *d = malloc(sizeof(DIR));
    if (!d) {
        errno = ENOMEM;
        return 0;
    }
    d->cursor = 0;
    d->dir_ino = dir_ino;
    d->fd = -1;
    return d;
}
DIR *fdopendir(int fd) {
    uint64_t dir_ino;
    if (__libc_fd_ramfs_dir_ino(fd, &dir_ino) != 0) {
        errno = ENOTDIR;
        return 0;
    }
    DIR *d = malloc(sizeof(DIR));
    if (!d) {
        errno = ENOMEM;
        return 0;
    }
    d->cursor = 0;
    d->dir_ino = dir_ino;
    d->fd = fd;
    return d;
}
struct dirent *readdir(DIR *dirp) {
    static struct dirent entry;
    char name[RAMFS_NAME_MAX];
    int is_dir;
    if (ramfs_readdir(dirp->dir_ino, dirp->cursor, name, &is_dir) != 0) {
        return 0;
    }
    dirp->cursor++;
    entry.d_ino = 1;
    entry.d_type = is_dir ? DT_DIR : DT_REG;
    strlcpy(entry.d_name, name, sizeof(entry.d_name));
    return &entry;
}
int closedir(DIR *dirp) {
    if (dirp->fd >= 0) {
        close(dirp->fd);
    }
    free(dirp);
    return 0;
}
int dirfd(DIR *dirp) {
    if (dirp->fd < 0) {
        errno = ENOSYS;
        return -1;
    }
    return dirp->fd;
}
void rewinddir(DIR *dirp) {
    dirp->cursor = 0;
}
