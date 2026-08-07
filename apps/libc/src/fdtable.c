#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/devfs.h"
#include "robu/ramfs.h"
#include "robu/procfs.h"
#include "robu/sysfs.h"
#include "robu/spawn.h"
#include "libc_internal.h"
#define MAX_FDS 64
#define RESOLVED_PATH_MAX 64
typedef enum {
    FD_NONE = 0,
    FD_DEVFS,
    FD_RAMFS,
    FD_RAMFS_ROOT,
    FD_PROCFS,
    FD_SYSFS,
    FD_PIPE_READ = SPAWN_FD_KIND_PIPE_READ,
    FD_PIPE_WRITE = SPAWN_FD_KIND_PIPE_WRITE,
} fd_kind_t;
typedef struct {
    fd_kind_t kind;
    uint64_t handle;
    char dir_name[RAMFS_PATH_MAX];
    uint64_t size;
} fd_entry_t;
static fd_entry_t fd_table[MAX_FDS] = {
    [STDIN_FILENO] = { .kind = FD_DEVFS, .handle = DEV_CONSOLE },
    [STDOUT_FILENO] = { .kind = FD_DEVFS, .handle = DEV_CONSOLE },
    [STDERR_FILENO] = { .kind = FD_DEVFS, .handle = DEV_CONSOLE },
};
static int alloc_fd(void) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_table[i].kind == FD_NONE) {
            return i;
        }
    }
    return -1;
}
static int fd_valid(int fd) {
    return fd >= 0 && fd < MAX_FDS && fd_table[fd].kind != FD_NONE;
}
int __libc_fd_is_open(int fd) {
    return fd_valid(fd);
}
int __libc_fd_is_ramfs_root(int fd) {
    return fd_valid(fd) && fd_table[fd].kind == FD_RAMFS_ROOT;
}
int __libc_fd_ramfs_dir_ino(int fd, uint64_t *out_ino) {
    if (!fd_valid(fd) || fd_table[fd].kind != FD_RAMFS_ROOT) {
        return -1;
    }
    *out_ino = fd_table[fd].handle;
    return 0;
}
int __libc_fd_alloc_pipe(uint64_t handle, int is_write_end) {
    int fd = alloc_fd();
    if (fd < 0) {
        return -1;
    }
    fd_table[fd].kind = is_write_end ? FD_PIPE_WRITE : FD_PIPE_READ;
    fd_table[fd].handle = handle;
    return fd;
}
int __libc_fd_export(int fd, uint32_t *out_kind, uint64_t *out_handle) {
    if (!fd_valid(fd)) {
        return -1;
    }
    *out_kind = (uint32_t)fd_table[fd].kind;
    *out_handle = fd_table[fd].handle;
    return 0;
}
void __libc_fd_inherit(uint64_t spawn_info) {
    if (!spawn_info) {
        return;
    }
    const robu_spawn_info_t *info = (const robu_spawn_info_t *)spawn_info;
    if (info->magic != SPAWN_INFO_MAGIC) {
        return;
    }
    const robu_spawn_fd_t *fds = (const robu_spawn_fd_t *)(info + 1);
    uint32_t nfds = info->nfds;
    if (nfds > SPAWN_FD_INFO_MAX) {
        nfds = SPAWN_FD_INFO_MAX;
    }
    for (uint32_t i = 0; i < nfds; i++) {
        int fd = (int)fds[i].fd;
        if (fd < 0 || fd > 2) {
            continue;
        }
        fd_table[fd].kind = (fd_kind_t)fds[i].kind;
        fd_table[fd].handle = fds[i].handle;
    }
}
#define CWD_MAX 256
static char g_cwd[CWD_MAX] = "/";
static void resolve_path(const char *path, char *out, size_t out_size);
static int ramfs_name_from_path(const char *resolved, char *name, size_t name_size);
int chdir(const char *path) {
    char resolved[RESOLVED_PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    int valid = (strcmp(resolved, "/") == 0 || strcmp(resolved, "/dev") == 0);
    if (!valid) {
        char name[RAMFS_PATH_MAX];
        uint64_t size, ino;
        int is_dir;
        if (ramfs_name_from_path(resolved, name, sizeof(name)) == 0
            && ramfs_stat(name, &size, &is_dir, &ino) == 0 && is_dir) {
            valid = 1;
        }
    }
    if (!valid) {
        errno = ENOTDIR;
        return -1;
    }
    size_t len = strlen(resolved);
    if (len >= CWD_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    memcpy(g_cwd, resolved, len + 1);
    return 0;
}
int fchdir(int fd) {
    (void)fd;
    errno = ENOSYS;
    return -1;
}
char *getcwd(char *buf, size_t size) {
    size_t len = strlen(g_cwd);
    if (!buf) {
        size = len + 1;
        buf = malloc(size);
        if (!buf) {
            errno = ENOMEM;
            return 0;
        }
    } else if (size < len + 1) {
        errno = ERANGE;
        return 0;
    }
    memcpy(buf, g_cwd, len + 1);
    return buf;
}
static void resolve_path(const char *path, char *out, size_t out_size) {
    char raw[RESOLVED_PATH_MAX];
    if (path[0] == '/') {
        strlcpy(raw, path, sizeof(raw));
    } else {
        size_t cwdlen = strlcpy(raw, g_cwd, sizeof(raw));
        if (cwdlen > 0 && cwdlen < sizeof(raw) && raw[cwdlen - 1] != '/') {
            strlcat(raw, "/", sizeof(raw));
        }
        strlcat(raw, path, sizeof(raw));
    }
    char *segs[16];
    int nseg = 0;
    char *save = NULL;
    for (char *tok = strtok_r(raw, "/", &save); tok; tok = strtok_r(NULL, "/", &save)) {
        if (strcmp(tok, ".") == 0) {
            continue;
        }
        if (strcmp(tok, "..") == 0) {
            if (nseg > 0) {
                nseg--;
            }
            continue;
        }
        if (nseg < (int)(sizeof(segs) / sizeof(segs[0]))) {
            segs[nseg++] = tok;
        }
    }
    size_t pos = 0;
    if (out_size > 1) {
        out[pos++] = '/';
    }
    out[pos] = '\0';
    for (int i = 0; i < nseg; i++) {
        size_t len = strlen(segs[i]);
        if (pos > 1) {
            if (pos + 1 >= out_size) break;
            out[pos++] = '/';
        }
        if (pos + len >= out_size) break;
        memcpy(out + pos, segs[i], len);
        pos += len;
        out[pos] = '\0';
    }
}
void __libc_resolve_path(const char *path, char *out, size_t out_size) {
    resolve_path(path, out, out_size);
}
static int resolve_at_path(int dirfd, const char *path, char *out, size_t out_size) {
    if (dirfd == AT_FDCWD) {
        resolve_path(path, out, out_size);
        return 0;
    }
    if (fd_valid(dirfd) && fd_table[dirfd].kind == FD_RAMFS_ROOT) {
        out[0] = '/';
        out[1] = '\0';
        const char *dn = fd_table[dirfd].dir_name;
        if (dn[0]) {
            strlcat(out, dn, out_size);
            strlcat(out, "/", out_size);
        }
        strlcat(out, path, out_size);
        return 0;
    }
    errno = ENOSYS;
    return -1;
}
static int ramfs_name_from_path(const char *resolved, char *name, size_t name_size) {
    const char *p = resolved;
    if (*p == '/') {
        p++;
    }
    if (*p == '\0') {
        errno = EISDIR;
        return -1;
    }
    if (strlen(p) >= RAMFS_PATH_MAX) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strlcpy(name, p, name_size);
    return 0;
}
int open(const char *path, int flags, ...) {
    char resolved[RESOLVED_PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (strncmp(resolved, "/dev/", 5) == 0) {
        int64_t h = devfs_open(resolved);
        if (h < 0) {
            errno = (h == DEVFS_ERR_NOT_FOUND) ? ENOENT : EIO;
            return -1;
        }
        int fd = alloc_fd();
        if (fd < 0) {
            devfs_close((uint64_t)h);
            errno = EMFILE;
            return -1;
        }
        fd_table[fd].kind = FD_DEVFS;
        fd_table[fd].handle = (uint64_t)h;
        return fd;
    }
    if (strncmp(resolved, "/proc/", 6) == 0) {
        uint64_t size;
        int64_t h = procfs_open(resolved + 6, &size);
        if (h < 0) {
            errno = (h == PROCFS_ERR_NOT_FOUND) ? ENOENT : EIO;
            return -1;
        }
        int fd = alloc_fd();
        if (fd < 0) {
            procfs_close((uint64_t)h);
            errno = EMFILE;
            return -1;
        }
        fd_table[fd].kind = FD_PROCFS;
        fd_table[fd].handle = (uint64_t)h;
        fd_table[fd].size = size;
        return fd;
    }
    if (strncmp(resolved, "/var/sys/", 9) == 0) {
        uint64_t size;
        int64_t h = sysfs_open(resolved + 9, &size);
        if (h < 0) {
            errno = (h == SYSFS_ERR_NOT_FOUND) ? ENOENT : EIO;
            return -1;
        }
        int fd = alloc_fd();
        if (fd < 0) {
            sysfs_close((uint64_t)h);
            errno = EMFILE;
            return -1;
        }
        fd_table[fd].kind = FD_SYSFS;
        fd_table[fd].handle = (uint64_t)h;
        fd_table[fd].size = size;
        return fd;
    }
    if (strcmp(resolved, "/") == 0) {
        if ((flags & O_ACCMODE) != O_RDONLY) {
            errno = EISDIR;
            return -1;
        }
        int fd = alloc_fd();
        if (fd < 0) {
            errno = EMFILE;
            return -1;
        }
        fd_table[fd].kind = FD_RAMFS_ROOT;
        fd_table[fd].handle = RAMFS_ROOT_INO;
        fd_table[fd].dir_name[0] = '\0';
        return fd;
    }
    char name[RAMFS_PATH_MAX];
    if (ramfs_name_from_path(resolved, name, sizeof(name)) != 0) {
        return -1;
    }
    {
        uint64_t st_size, st_ino;
        int st_is_dir;
        if (ramfs_stat(name, &st_size, &st_is_dir, &st_ino) == 0 && st_is_dir) {
            if ((flags & O_ACCMODE) != O_RDONLY) {
                errno = EISDIR;
                return -1;
            }
            int fd = alloc_fd();
            if (fd < 0) {
                errno = EMFILE;
                return -1;
            }
            fd_table[fd].kind = FD_RAMFS_ROOT;
            fd_table[fd].handle = st_ino;
            strlcpy(fd_table[fd].dir_name, name, sizeof(fd_table[fd].dir_name));
            return fd;
        }
    }
    uint64_t rflags = 0;
    if (flags & O_CREAT) {
        rflags |= RAMFS_O_CREAT;
    }
    if (flags & O_TRUNC) {
        rflags |= RAMFS_O_TRUNC;
    }
    if (flags & O_APPEND) {
        rflags |= RAMFS_O_APPEND;
    }
    int64_t h = ramfs_open(name, rflags);
    if (h < 0) {
        errno = (h == RAMFS_ERR_NOT_FOUND) ? ENOENT
              : (h == RAMFS_ERR_NO_SPACE) ? ENOSPC : EIO;
        return -1;
    }
    int fd = alloc_fd();
    if (fd < 0) {
        ramfs_close((uint64_t)h);
        errno = EMFILE;
        return -1;
    }
    fd_table[fd].kind = FD_RAMFS;
    fd_table[fd].handle = (uint64_t)h;
    return fd;
}
int openat(int dirfd, const char *path, int flags, ...) {
    char resolved[RESOLVED_PATH_MAX];
    if (resolve_at_path(dirfd, path, resolved, sizeof(resolved)) != 0) {
        return -1;
    }
    return open(resolved, flags);
}
int creat(const char *path, int mode) {
    (void)mode;
    return open(path, O_WRONLY | O_CREAT | O_TRUNC);
}
static int fd_shared_elsewhere(int fd) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (i != fd && fd_table[i].kind == fd_table[fd].kind
            && fd_table[i].handle == fd_table[fd].handle) {
            return 1;
        }
    }
    return 0;
}
int close(int fd) {
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    if (!fd_shared_elsewhere(fd)) {
        if (fd_table[fd].kind == FD_DEVFS) {
            devfs_close(fd_table[fd].handle);
        } else if (fd_table[fd].kind == FD_RAMFS) {
            ramfs_close(fd_table[fd].handle);
        } else if (fd_table[fd].kind == FD_PROCFS) {
            procfs_close(fd_table[fd].handle);
        } else if (fd_table[fd].kind == FD_SYSFS) {
            sysfs_close(fd_table[fd].handle);
        } else if (fd_table[fd].kind == FD_PIPE_READ) {
            msg_regs_t m = (msg_regs_t){0};
            m.word[0] = fd_table[fd].handle;
            m.word[1] = 0;
            robu_ipc_raw(0, 0, IPC_FLAG_PIPE_CLOSE, &m, NULL);
        } else if (fd_table[fd].kind == FD_PIPE_WRITE) {
            msg_regs_t m = (msg_regs_t){0};
            m.word[0] = fd_table[fd].handle;
            m.word[1] = 1;
            robu_ipc_raw(0, 0, IPC_FLAG_PIPE_CLOSE, &m, NULL);
        }
    }
    fd_table[fd].kind = FD_NONE;
    return 0;
}
#define PIPE_CHUNK_MAX 32
static int64_t pipe_read_raw(uint64_t handle, uint8_t *buf, uint64_t max, int *out_eof) {
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = handle;
    m.word[1] = max > PIPE_CHUNK_MAX ? PIPE_CHUNK_MAX : max;
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_PIPE_READ, &m, NULL);
    if (rc == IPC_ERR_NOT_FOUND) {
        *out_eof = 1;
        return 0;
    }
    *out_eof = 0;
    if (rc != IPC_ERR_NONE) {
        return -1;
    }
    uint64_t n = m.word[0];
    uint64_t words[4] = { m.word[2], m.word[3], m.word[4], m.word[5] };
    const uint8_t *bytes = (const uint8_t *)words;
    for (uint64_t i = 0; i < n; i++) {
        buf[i] = bytes[i];
    }
    return (int64_t)n;
}
static int64_t pipe_write_raw(uint64_t handle, const uint8_t *buf, uint64_t len, int *out_broken) {
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = handle;
    uint64_t n = len > PIPE_CHUNK_MAX ? PIPE_CHUNK_MAX : len;
    m.word[1] = n;
    uint64_t words[4] = {0, 0, 0, 0};
    uint8_t *bytes = (uint8_t *)words;
    for (uint64_t i = 0; i < n; i++) {
        bytes[i] = buf[i];
    }
    m.word[2] = words[0];
    m.word[3] = words[1];
    m.word[4] = words[2];
    m.word[5] = words[3];
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_PIPE_WRITE, &m, NULL);
    if (rc == IPC_ERR_NOT_FOUND) {
        *out_broken = 1;
        return 0;
    }
    *out_broken = 0;
    if (rc != IPC_ERR_NONE) {
        return -1;
    }
    return (int64_t)m.word[0];
}
ssize_t write(int fd, const void *buf, size_t count) {
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    if (fd_table[fd].kind == FD_RAMFS_ROOT) {
        errno = EISDIR;
        return -1;
    }
    if (fd_table[fd].kind == FD_PIPE_READ) {
        errno = EBADF;
        return -1;
    }
    if (fd_table[fd].kind == FD_PROCFS || fd_table[fd].kind == FD_SYSFS) {
        errno = EBADF;
        return -1;
    }
    if (fd_table[fd].kind == FD_PIPE_WRITE) {
        uint64_t handle = fd_table[fd].handle;
        const uint8_t *p = buf;
        size_t off = 0;
        while (off < count) {
            int broken = 0;
            int64_t n = pipe_write_raw(handle, p + off, count - off, &broken);
            if (broken) {
                errno = EPIPE;
                return off ? (ssize_t)off : -1;
            }
            if (n < 0) {
                errno = EIO;
                return off ? (ssize_t)off : -1;
            }
            if (n == 0) {
                ipc_sleep(1);
                continue;
            }
            off += (size_t)n;
        }
        return (ssize_t)off;
    }
    uint64_t handle = fd_table[fd].handle;
    int is_devfs = fd_table[fd].kind == FD_DEVFS;
    size_t max_chunk = is_devfs ? DEVFS_WRITE_MAX : RAMFS_WRITE_MAX;
    const uint8_t *p = buf;
    size_t off = 0;
    while (off < count) {
        size_t chunk = count - off;
        if (chunk > max_chunk) {
            chunk = max_chunk;
        }
        int64_t n = is_devfs ? devfs_write(handle, p + off, chunk)
                             : ramfs_write(handle, p + off, chunk);
        if (n < 0) {
            errno = (n == RAMFS_ERR_NO_SPACE) ? ENOSPC : EIO;
            return off ? (ssize_t)off : -1;
        }
        off += (size_t)n;
        if ((size_t)n < chunk) {
            break;
        }
    }
    return (ssize_t)off;
}
ssize_t read(int fd, void *buf, size_t count) {
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    if (fd_table[fd].kind == FD_RAMFS_ROOT) {
        errno = EISDIR;
        return -1;
    }
    if (fd_table[fd].kind == FD_PIPE_WRITE) {
        errno = EBADF;
        return -1;
    }
    if (fd_table[fd].kind == FD_PIPE_READ) {
        uint64_t handle = fd_table[fd].handle;
        uint8_t *p = buf;
        size_t off = 0;
        while (off < count) {
            int eof = 0;
            int64_t n = pipe_read_raw(handle, p + off, count - off, &eof);
            if (n < 0) {
                errno = EIO;
                return off ? (ssize_t)off : -1;
            }
            if (n == 0) {
                if (eof || off > 0) {
                    break;
                }
                ipc_sleep(1);
                continue;
            }
            off += (size_t)n;
            break;
        }
        return (ssize_t)off;
    }
    uint64_t handle = fd_table[fd].handle;
    fd_kind_t kind = fd_table[fd].kind;
    int is_devfs = kind == FD_DEVFS;
    int is_console = is_devfs && handle == DEV_CONSOLE;
    size_t max_chunk = is_devfs ? DEVFS_READ_MAX
                      : kind == FD_PROCFS ? PROCFS_READ_MAX
                      : kind == FD_SYSFS ? SYSFS_READ_MAX
                      : RAMFS_READ_MAX;
    uint8_t *p = buf;
    size_t off = 0;
    while (off < count) {
        size_t chunk = count - off;
        if (chunk > max_chunk) {
            chunk = max_chunk;
        }
        int64_t n = is_devfs ? devfs_read(handle, p + off, chunk)
                  : kind == FD_PROCFS ? procfs_read(handle, p + off, chunk)
                  : kind == FD_SYSFS ? sysfs_read(handle, p + off, chunk)
                  : ramfs_read(handle, p + off, chunk);
        if (n < 0) {
            errno = EIO;
            return off ? (ssize_t)off : -1;
        }
        if (n == 0) {
            if (is_console) {
                if (off > 0) {
                    break;
                }
                ipc_sleep(1);
                continue;
            }
            break;
        }
        off += (size_t)n;
        if ((size_t)n < chunk) {
            break;
        }
    }
    return (ssize_t)off;
}
int dup(int fd) {
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    int newfd = alloc_fd();
    if (newfd < 0) {
        errno = EMFILE;
        return -1;
    }
    fd_table[newfd] = fd_table[fd];
    return newfd;
}
int dup2(int oldfd, int newfd) {
    if (!fd_valid(oldfd) || newfd < 0 || newfd >= MAX_FDS) {
        errno = EBADF;
        return -1;
    }
    if (newfd == oldfd) {
        return newfd;
    }
    if (fd_table[newfd].kind != FD_NONE) {
        close(newfd);
    }
    fd_table[newfd] = fd_table[oldfd];
    return newfd;
}
int fcntl(int fd, int cmd, ...) {
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    switch (cmd) {
    case F_GETFL:
        return 0;
    case F_SETFD:
        return 0;
    default:
        errno = ENOSYS;
        return -1;
    }
}
off_t lseek(int fd, off_t offset, int whence) {
    (void)offset;
    (void)whence;
    if (!fd_valid(fd)) {
        errno = EBADF;
        return (off_t)-1;
    }
    errno = ESPIPE;
    return (off_t)-1;
}
int isatty(int fd) {
    return fd_valid(fd) && fd_table[fd].kind == FD_DEVFS
        && fd_table[fd].handle == DEV_CONSOLE;
}
char *ttyname(int fd) {
    if (!isatty(fd)) {
        errno = ENOTTY;
        return 0;
    }
    return (char *)"/dev/console";
}
int fsync(int fd) {
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    return 0;
}
void sync(void) {
}
int access(const char *path, int mode) {
    (void)mode;
    char resolved[RESOLVED_PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (strncmp(resolved, "/dev/", 5) == 0) {
        int64_t h = devfs_open(resolved);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        devfs_close((uint64_t)h);
        return 0;
    }
    if (strncmp(resolved, "/proc/", 6) == 0) {
        int64_t h = procfs_open(resolved + 6, 0);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        procfs_close((uint64_t)h);
        return 0;
    }
    if (strncmp(resolved, "/var/sys/", 9) == 0) {
        int64_t h = sysfs_open(resolved + 9, 0);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        sysfs_close((uint64_t)h);
        return 0;
    }
    if (strcmp(resolved, "/") == 0) {
        return 0;
    }
    char name[RAMFS_PATH_MAX];
    if (ramfs_name_from_path(resolved, name, sizeof(name)) != 0) {
        return -1;
    }
    uint64_t size;
    int is_dir;
    if (ramfs_stat(name, &size, &is_dir, 0) != 0) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
int faccessat(int dirfd, const char *path, int mode, int flags) {
    (void)flags;
    char resolved[RESOLVED_PATH_MAX];
    if (resolve_at_path(dirfd, path, resolved, sizeof(resolved)) != 0) {
        return -1;
    }
    return access(resolved, mode);
}
#define ROBU_STDEV_DEVFS  1
#define ROBU_STDEV_RAMFS  2
#define ROBU_STDEV_PROCFS 3
#define ROBU_STDEV_SYSFS  4
static void fill_stat(struct stat *buf, uint64_t size, int is_dir, dev_t dev, ino_t ino) {
    memset(buf, 0, sizeof(*buf));
    buf->st_mode = is_dir ? (S_IFDIR | 0755) : (S_IFREG | 0644);
    buf->st_size = (off_t)size;
    buf->st_nlink = 1;
    buf->st_blksize = 512;
    buf->st_blocks = (blkcnt_t)((size + 511) / 512);
    buf->st_dev = dev;
    buf->st_ino = ino;
}
int stat(const char *path, struct stat *buf) {
    char resolved[RESOLVED_PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (strncmp(resolved, "/dev/", 5) == 0) {
        int64_t h = devfs_open(resolved);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        devfs_close((uint64_t)h);
        fill_stat(buf, 0, 0, ROBU_STDEV_DEVFS, (ino_t)h + 1);
        buf->st_mode = S_IFCHR | 0666;
        return 0;
    }
    if (strncmp(resolved, "/proc/", 6) == 0) {
        uint64_t size = 0;
        int64_t h = procfs_open(resolved + 6, &size);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        procfs_close((uint64_t)h);
        fill_stat(buf, size, 0, ROBU_STDEV_PROCFS, (ino_t)h + 1);
        return 0;
    }
    if (strncmp(resolved, "/var/sys/", 9) == 0) {
        uint64_t size = 0;
        int64_t h = sysfs_open(resolved + 9, &size);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        sysfs_close((uint64_t)h);
        fill_stat(buf, size, 0, ROBU_STDEV_SYSFS, (ino_t)h + 1);
        return 0;
    }
    if (strcmp(resolved, "/") == 0) {
        fill_stat(buf, 0, 1, ROBU_STDEV_RAMFS, 1);
        return 0;
    }
    char name[RAMFS_PATH_MAX];
    if (ramfs_name_from_path(resolved, name, sizeof(name)) != 0) {
        return -1;
    }
    uint64_t size, ino;
    int is_dir;
    if (ramfs_stat(name, &size, &is_dir, &ino) != 0) {
        errno = ENOENT;
        return -1;
    }
    fill_stat(buf, size, is_dir, ROBU_STDEV_RAMFS, (ino_t)ino);
    return 0;
}
int lstat(const char *path, struct stat *buf) {
    return stat(path, buf);
}
int fstat(int fd, struct stat *buf) {
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    if (fd_table[fd].kind == FD_DEVFS) {
        fill_stat(buf, 0, 0, ROBU_STDEV_DEVFS, (ino_t)fd_table[fd].handle + 1);
        buf->st_mode = S_IFCHR | 0666;
        return 0;
    }
    if (fd_table[fd].kind == FD_RAMFS_ROOT) {
        fill_stat(buf, 0, 1, ROBU_STDEV_RAMFS, (ino_t)fd_table[fd].handle);
        return 0;
    }
    if (fd_table[fd].kind == FD_PROCFS) {
        fill_stat(buf, fd_table[fd].size, 0, ROBU_STDEV_PROCFS, (ino_t)fd_table[fd].handle + 1);
        return 0;
    }
    if (fd_table[fd].kind == FD_SYSFS) {
        fill_stat(buf, fd_table[fd].size, 0, ROBU_STDEV_SYSFS, (ino_t)fd_table[fd].handle + 1);
        return 0;
    }
    uint64_t size, ino;
    int is_dir;
    if (ramfs_fstat(fd_table[fd].handle, &size, &is_dir, &ino) != 0) {
        errno = EBADF;
        return -1;
    }
    fill_stat(buf, size, is_dir, ROBU_STDEV_RAMFS, (ino_t)ino);
    return 0;
}
int fstatat(int dirfd, const char *path, struct stat *buf, int flags) {
    (void)flags;
    char resolved[RESOLVED_PATH_MAX];
    if (resolve_at_path(dirfd, path, resolved, sizeof(resolved)) != 0) {
        return -1;
    }
    return stat(resolved, buf);
}
int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags) {
    (void)times;
    (void)flags;
    if (dirfd != AT_FDCWD) {
        errno = ENOSYS;
        return -1;
    }
    char resolved[RESOLVED_PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (strncmp(resolved, "/dev/", 5) == 0) {
        int64_t h = devfs_open(resolved);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        devfs_close((uint64_t)h);
        return 0;
    }
    if (strncmp(resolved, "/proc/", 6) == 0) {
        int64_t h = procfs_open(resolved + 6, 0);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        procfs_close((uint64_t)h);
        return 0;
    }
    if (strncmp(resolved, "/var/sys/", 9) == 0) {
        int64_t h = sysfs_open(resolved + 9, 0);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        sysfs_close((uint64_t)h);
        return 0;
    }
    char name[RAMFS_PATH_MAX];
    if (ramfs_name_from_path(resolved, name, sizeof(name)) != 0) {
        return -1;
    }
    uint64_t size;
    int is_dir;
    if (ramfs_stat(name, &size, &is_dir, 0) != 0) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
int futimens(int fd, const struct timespec times[2]) {
    (void)times;
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    return 0;
}
int chmod(const char *path, mode_t mode) {
    (void)mode;
    char resolved[RESOLVED_PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (strncmp(resolved, "/dev/", 5) == 0) {
        int64_t h = devfs_open(resolved);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        devfs_close((uint64_t)h);
        return 0;
    }
    if (strncmp(resolved, "/proc/", 6) == 0) {
        int64_t h = procfs_open(resolved + 6, 0);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        procfs_close((uint64_t)h);
        return 0;
    }
    if (strncmp(resolved, "/var/sys/", 9) == 0) {
        int64_t h = sysfs_open(resolved + 9, 0);
        if (h < 0) {
            errno = ENOENT;
            return -1;
        }
        sysfs_close((uint64_t)h);
        return 0;
    }
    char name[RAMFS_PATH_MAX];
    if (ramfs_name_from_path(resolved, name, sizeof(name)) != 0) {
        return -1;
    }
    uint64_t size;
    int is_dir;
    if (ramfs_stat(name, &size, &is_dir, 0) != 0) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
int fchmod(int fd, mode_t mode) {
    (void)mode;
    if (!fd_valid(fd)) {
        errno = EBADF;
        return -1;
    }
    return 0;
}
int fchmodat(int dirfd, const char *path, mode_t mode, int flags) {
    (void)flags;
    char resolved[RESOLVED_PATH_MAX];
    if (resolve_at_path(dirfd, path, resolved, sizeof(resolved)) != 0) {
        return -1;
    }
    return chmod(resolved, mode);
}
int mkdir(const char *path, mode_t mode) {
    (void)path;
    (void)mode;
    errno = ENOSYS;
    return -1;
}
int mkdirat(int dirfd, const char *path, mode_t mode) {
    (void)dirfd;
    (void)path;
    (void)mode;
    errno = ENOSYS;
    return -1;
}
int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev) {
    (void)dirfd;
    (void)path;
    (void)mode;
    (void)dev;
    errno = ENOSYS;
    return -1;
}
int mknod(const char *path, mode_t mode, dev_t dev) {
    return mknodat(AT_FDCWD, path, mode, dev);
}
int utime(const char *path, const struct utimbuf *times) {
    (void)times;
    if (access(path, F_OK) != 0) {
        return -1;
    }
    return 0;
}
int rename(const char *oldpath, const char *newpath) {
    char roldpath[RESOLVED_PATH_MAX], rnewpath[RESOLVED_PATH_MAX];
    resolve_path(oldpath, roldpath, sizeof(roldpath));
    resolve_path(newpath, rnewpath, sizeof(rnewpath));
    if (strncmp(roldpath, "/dev/", 5) == 0 || strncmp(rnewpath, "/dev/", 5) == 0) {
        errno = ENOSYS;
        return -1;
    }
    if (strncmp(roldpath, "/proc/", 6) == 0 || strncmp(rnewpath, "/proc/", 6) == 0
        || strncmp(roldpath, "/var/sys/", 9) == 0 || strncmp(rnewpath, "/var/sys/", 9) == 0) {
        errno = ENOSYS;
        return -1;
    }
    char oldname[RAMFS_NAME_MAX], newname[RAMFS_NAME_MAX];
    if (ramfs_name_from_path(roldpath, oldname, sizeof(oldname)) != 0
        || ramfs_name_from_path(rnewpath, newname, sizeof(newname)) != 0) {
        return -1;
    }
    if (ramfs_rename(oldname, newname) != 0) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
int unlink(const char *path) {
    char resolved[RESOLVED_PATH_MAX];
    resolve_path(path, resolved, sizeof(resolved));
    if (strncmp(resolved, "/dev/", 5) == 0) {
        errno = ENOSYS;
        return -1;
    }
    if (strncmp(resolved, "/proc/", 6) == 0 || strncmp(resolved, "/var/sys/", 9) == 0) {
        errno = ENOSYS;
        return -1;
    }
    char name[RAMFS_PATH_MAX];
    if (ramfs_name_from_path(resolved, name, sizeof(name)) != 0) {
        return -1;
    }
    if (ramfs_unlink(name) != 0) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}
int unlinkat(int dirfd, const char *path, int flags) {
    (void)flags;
    char resolved[RESOLVED_PATH_MAX];
    if (resolve_at_path(dirfd, path, resolved, sizeof(resolved)) != 0) {
        return -1;
    }
    return unlink(resolved);
}
