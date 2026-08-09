#ifndef ROBU_VFS_H
#define ROBU_VFS_H
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
// A single generic filesystem-server protocol, formalizing what
// include/robu/ramfs.h (the richest of the four original bespoke
// protocols) already established -- every op/error code number below is
// unchanged from ramfs.h on purpose, so ramfs's own migration is a pure
// rename. devfs/procfs/sysfs implement a subset (no real directories, no
// write for procfs/sysfs) and answer VFS_ERR_NOT_SUPPORTED for the rest --
// this mirrors how ramfs.h's own RAMFS_ERR_NOT_SUPPORTED was already used.
//
// Unlike ramfs.h's client wrappers (which hardcode ramfs_server_tid()),
// every wrapper here takes an explicit `tid_t server` -- the whole point of
// the mount table (include/robu/kinfo.h's kinfo_resolve_mount()) is that
// which server owns a given path is resolved dynamically at the call site,
// not fixed per protocol.
#define VFS_OP_OPEN    1
#define VFS_OP_READ    2
#define VFS_OP_WRITE   3
#define VFS_OP_CLOSE   4
#define VFS_OP_STAT    5
#define VFS_OP_FSTAT   6
#define VFS_OP_READDIR 7
#define VFS_OP_RENAME  8
#define VFS_OP_UNLINK  9
#define VFS_OP_SYMLINK 10
#define VFS_ERR_NOT_FOUND     (-1)
#define VFS_ERR_BAD_HANDLE    (-2)
#define VFS_ERR_NOT_SUPPORTED (-3)
#define VFS_ERR_NO_SPACE      (-4)
#define VFS_ERR_IS_DIR        (-5)
#define VFS_NAME_MAX  20
#define VFS_PATH_MAX  32
#define VFS_READ_MAX  40
#define VFS_WRITE_MAX 24
#define VFS_ROOT_INO 1
#define VFS_O_CREAT  0x0040
#define VFS_O_TRUNC  0x0200
#define VFS_O_APPEND 0x0400
typedef struct {
    uint64_t op;
    uint64_t flags;
    char name[VFS_PATH_MAX];
} vfs_open_req_t;
_Static_assert(sizeof(vfs_open_req_t) <= 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
    uint64_t handle;
} vfs_open_reply_t;
typedef struct {
    uint64_t op;
    uint64_t handle;
    uint64_t len;
} vfs_read_req_t;
typedef struct {
    int64_t status;
    uint8_t data[VFS_READ_MAX];
} vfs_read_reply_t;
_Static_assert(sizeof(vfs_read_reply_t) == 48, "must fit one msg_regs_t");
typedef struct {
    uint64_t op;
    uint64_t handle;
    uint64_t len;
    uint8_t data[VFS_WRITE_MAX];
} vfs_write_req_t;
_Static_assert(sizeof(vfs_write_req_t) == 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
} vfs_write_reply_t;
typedef struct {
    uint64_t op;
    uint64_t handle;
} vfs_close_req_t;
typedef struct {
    int64_t status;
} vfs_close_reply_t;
typedef struct {
    uint64_t op;
    char name[VFS_PATH_MAX];
} vfs_stat_req_t;
_Static_assert(sizeof(vfs_stat_req_t) <= 48, "must fit one msg_regs_t");
typedef struct {
    uint64_t op;
    uint64_t handle;
} vfs_fstat_req_t;
typedef struct {
    int64_t status;
    uint64_t size;
    uint64_t is_dir;
    uint64_t ino;
} vfs_stat_reply_t;
typedef struct {
    uint64_t op;
    uint64_t dir_ino;
    uint64_t index;
} vfs_readdir_req_t;
typedef struct {
    int64_t status;
    uint64_t is_dir;
    char name[VFS_NAME_MAX];
} vfs_readdir_reply_t;
typedef struct {
    uint64_t op;
    char oldname[VFS_NAME_MAX];
    char newname[VFS_NAME_MAX];
} vfs_rename_req_t;
_Static_assert(sizeof(vfs_rename_req_t) == 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
} vfs_rename_reply_t;
typedef struct {
    uint64_t op;
    char name[VFS_PATH_MAX];
} vfs_unlink_req_t;
_Static_assert(sizeof(vfs_unlink_req_t) <= 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
} vfs_unlink_reply_t;
typedef struct {
    uint64_t op;
    char name[VFS_NAME_MAX];
    char target[VFS_NAME_MAX];
} vfs_symlink_req_t;
_Static_assert(sizeof(vfs_symlink_req_t) == 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
} vfs_symlink_reply_t;
// Registers the caller as the owner of `prefix` in the kernel-resident
// mount table (kinfo_page_t.mounts[], include/robu/kinfo.h). Gated
// kernel-side to boot-spawned servers only (see IPC_FLAG_MOUNT's handler in
// src/core/ipc.c) -- a server calls this once, early in its own _start(),
// the same way it currently might report its own tid.
static inline int64_t vfs_mount(const char *prefix) {
    msg_regs_t m = (msg_regs_t){0};
    uint64_t len = 0;
    while (prefix[len] && len < MOUNT_PREFIX_MAX - 1) {
        len++;
    }
    m.word[0] = len;
    uint64_t words[5] = {0, 0, 0, 0, 0};
    for (uint64_t i = 0; i < len; i++) {
        ((uint8_t *)words)[i] = (uint8_t)prefix[i];
    }
    m.word[1] = words[0];
    m.word[2] = words[1];
    m.word[3] = words[2];
    m.word[4] = words[3];
    m.word[5] = words[4];
    return robu_ipc_raw(0, 0, IPC_FLAG_MOUNT, &m, NULL);
}
static inline int64_t vfs_open(tid_t server, const char *path, uint64_t flags) {
    msg_regs_t m;
    vfs_open_req_t *req = (vfs_open_req_t *)&m;
    req->op = VFS_OP_OPEN;
    req->flags = flags;
    size_t i = 0;
    while (path[i] && i < VFS_PATH_MAX - 1) {
        req->name[i] = path[i];
        i++;
    }
    req->name[i] = '\0';
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_open_reply_t *reply = (vfs_open_reply_t *)&m;
    return reply->status == 0 ? (int64_t)reply->handle : reply->status;
}
static inline int64_t vfs_read(tid_t server, uint64_t handle, void *buf, uint64_t len) {
    msg_regs_t m;
    vfs_read_req_t *req = (vfs_read_req_t *)&m;
    req->op = VFS_OP_READ;
    req->handle = handle;
    req->len = len > VFS_READ_MAX ? VFS_READ_MAX : len;
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_read_reply_t *reply = (vfs_read_reply_t *)&m;
    if (reply->status > 0) {
        uint8_t *out = (uint8_t *)buf;
        for (int64_t i = 0; i < reply->status; i++) {
            out[i] = reply->data[i];
        }
    }
    return reply->status;
}
static inline int64_t vfs_write(tid_t server, uint64_t handle, const void *buf, uint64_t len) {
    msg_regs_t m;
    vfs_write_req_t *req = (vfs_write_req_t *)&m;
    req->op = VFS_OP_WRITE;
    req->handle = handle;
    req->len = len > VFS_WRITE_MAX ? VFS_WRITE_MAX : len;
    const uint8_t *in = (const uint8_t *)buf;
    for (uint64_t i = 0; i < req->len; i++) {
        req->data[i] = in[i];
    }
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_write_reply_t *reply = (vfs_write_reply_t *)&m;
    return reply->status;
}
static inline int64_t vfs_close(tid_t server, uint64_t handle) {
    msg_regs_t m;
    vfs_close_req_t *req = (vfs_close_req_t *)&m;
    req->op = VFS_OP_CLOSE;
    req->handle = handle;
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_close_reply_t *reply = (vfs_close_reply_t *)&m;
    return reply->status;
}
static inline int64_t vfs_stat(tid_t server, const char *name, uint64_t *size_out,
                               int *is_dir_out, uint64_t *ino_out) {
    msg_regs_t m;
    vfs_stat_req_t *req = (vfs_stat_req_t *)&m;
    req->op = VFS_OP_STAT;
    size_t i = 0;
    while (name[i] && i < VFS_PATH_MAX - 1) {
        req->name[i] = name[i];
        i++;
    }
    req->name[i] = '\0';
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_stat_reply_t *reply = (vfs_stat_reply_t *)&m;
    if (reply->status == 0) {
        if (size_out) *size_out = reply->size;
        if (is_dir_out) *is_dir_out = (int)reply->is_dir;
        if (ino_out) *ino_out = reply->ino;
    }
    return reply->status;
}
static inline int64_t vfs_fstat(tid_t server, uint64_t handle, uint64_t *size_out,
                                int *is_dir_out, uint64_t *ino_out) {
    msg_regs_t m;
    vfs_fstat_req_t *req = (vfs_fstat_req_t *)&m;
    req->op = VFS_OP_FSTAT;
    req->handle = handle;
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_stat_reply_t *reply = (vfs_stat_reply_t *)&m;
    if (reply->status == 0) {
        if (size_out) *size_out = reply->size;
        if (is_dir_out) *is_dir_out = (int)reply->is_dir;
        if (ino_out) *ino_out = reply->ino;
    }
    return reply->status;
}
static inline int64_t vfs_readdir(tid_t server, uint64_t dir_ino, uint64_t index,
                                  char *name_out, int *is_dir_out) {
    msg_regs_t m;
    vfs_readdir_req_t *req = (vfs_readdir_req_t *)&m;
    req->op = VFS_OP_READDIR;
    req->dir_ino = dir_ino;
    req->index = index;
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_readdir_reply_t *reply = (vfs_readdir_reply_t *)&m;
    if (reply->status == 0) {
        size_t i = 0;
        for (; i < VFS_NAME_MAX - 1 && reply->name[i]; i++) {
            name_out[i] = reply->name[i];
        }
        name_out[i] = '\0';
        if (is_dir_out) *is_dir_out = (int)reply->is_dir;
    }
    return reply->status;
}
static inline int64_t vfs_rename(tid_t server, const char *oldname, const char *newname) {
    msg_regs_t m;
    vfs_rename_req_t *req = (vfs_rename_req_t *)&m;
    req->op = VFS_OP_RENAME;
    size_t i = 0;
    while (oldname[i] && i < VFS_NAME_MAX - 1) {
        req->oldname[i] = oldname[i];
        i++;
    }
    req->oldname[i] = '\0';
    i = 0;
    while (newname[i] && i < VFS_NAME_MAX - 1) {
        req->newname[i] = newname[i];
        i++;
    }
    req->newname[i] = '\0';
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_rename_reply_t *reply = (vfs_rename_reply_t *)&m;
    return reply->status;
}
static inline int64_t vfs_unlink(tid_t server, const char *name) {
    msg_regs_t m;
    vfs_unlink_req_t *req = (vfs_unlink_req_t *)&m;
    req->op = VFS_OP_UNLINK;
    size_t i = 0;
    while (name[i] && i < VFS_PATH_MAX - 1) {
        req->name[i] = name[i];
        i++;
    }
    req->name[i] = '\0';
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_unlink_reply_t *reply = (vfs_unlink_reply_t *)&m;
    return reply->status;
}
static inline int64_t vfs_symlink(tid_t server, const char *name, const char *target) {
    msg_regs_t m;
    vfs_symlink_req_t *req = (vfs_symlink_req_t *)&m;
    req->op = VFS_OP_SYMLINK;
    size_t i = 0;
    while (name[i] && i < VFS_NAME_MAX - 1) {
        req->name[i] = name[i];
        i++;
    }
    req->name[i] = '\0';
    i = 0;
    while (target[i] && i < VFS_NAME_MAX - 1) {
        req->target[i] = target[i];
        i++;
    }
    req->target[i] = '\0';
    tid_t from;
    ipc_call(server, &m, &from);
    vfs_symlink_reply_t *reply = (vfs_symlink_reply_t *)&m;
    return reply->status;
}
#endif
