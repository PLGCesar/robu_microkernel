#ifndef ROBU_RAMFS_H
#define ROBU_RAMFS_H
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#define RAMFS_OP_OPEN    1
#define RAMFS_OP_READ    2
#define RAMFS_OP_WRITE   3
#define RAMFS_OP_CLOSE   4
#define RAMFS_OP_STAT    5
#define RAMFS_OP_FSTAT   6
#define RAMFS_OP_READDIR 7
#define RAMFS_OP_RENAME  8
#define RAMFS_OP_UNLINK  9
#define RAMFS_ERR_NOT_FOUND     (-1)
#define RAMFS_ERR_BAD_HANDLE    (-2)
#define RAMFS_ERR_NOT_SUPPORTED (-3)
#define RAMFS_ERR_NO_SPACE      (-4)
#define RAMFS_ERR_IS_DIR        (-5)
#define RAMFS_NAME_MAX  20
#define RAMFS_PATH_MAX  32
#define RAMFS_READ_MAX  40
#define RAMFS_WRITE_MAX 24
#define RAMFS_ROOT_INO 1
#define RAMFS_O_CREAT  0x0040
#define RAMFS_O_TRUNC  0x0200
#define RAMFS_O_APPEND 0x0400
typedef struct {
    uint64_t op;
    uint64_t flags;
    char name[RAMFS_PATH_MAX];
} ramfs_open_req_t;
_Static_assert(sizeof(ramfs_open_req_t) <= 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
    uint64_t handle;
} ramfs_open_reply_t;
typedef struct {
    uint64_t op;
    uint64_t handle;
    uint64_t len;
} ramfs_read_req_t;
typedef struct {
    int64_t status;
    uint8_t data[RAMFS_READ_MAX];
} ramfs_read_reply_t;
_Static_assert(sizeof(ramfs_read_reply_t) == 48, "must fit one msg_regs_t");
typedef struct {
    uint64_t op;
    uint64_t handle;
    uint64_t len;
    uint8_t data[RAMFS_WRITE_MAX];
} ramfs_write_req_t;
_Static_assert(sizeof(ramfs_write_req_t) == 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
} ramfs_write_reply_t;
typedef struct {
    uint64_t op;
    uint64_t handle;
} ramfs_close_req_t;
typedef struct {
    int64_t status;
} ramfs_close_reply_t;
typedef struct {
    uint64_t op;
    char name[RAMFS_PATH_MAX];
} ramfs_stat_req_t;
_Static_assert(sizeof(ramfs_stat_req_t) <= 48, "must fit one msg_regs_t");
typedef struct {
    uint64_t op;
    uint64_t handle;
} ramfs_fstat_req_t;
typedef struct {
    int64_t status;
    uint64_t size;
    uint64_t is_dir;
    uint64_t ino;
} ramfs_stat_reply_t;
typedef struct {
    uint64_t op;
    uint64_t dir_ino;
    uint64_t index;
} ramfs_readdir_req_t;
typedef struct {
    int64_t status;
    uint64_t is_dir;
    char name[RAMFS_NAME_MAX];
} ramfs_readdir_reply_t;
typedef struct {
    uint64_t op;
    char oldname[RAMFS_NAME_MAX];
    char newname[RAMFS_NAME_MAX];
} ramfs_rename_req_t;
_Static_assert(sizeof(ramfs_rename_req_t) == 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
} ramfs_rename_reply_t;
typedef struct {
    uint64_t op;
    char name[RAMFS_PATH_MAX];
} ramfs_unlink_req_t;
_Static_assert(sizeof(ramfs_unlink_req_t) <= 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
} ramfs_unlink_reply_t;
static inline tid_t ramfs_server_tid(void) {
    return (tid_t)kinfo_user()->ramfs_tid;
}
static inline int64_t ramfs_open(const char *name, uint64_t flags) {
    msg_regs_t m;
    ramfs_open_req_t *req = (ramfs_open_req_t *)&m;
    req->op = RAMFS_OP_OPEN;
    req->flags = flags;
    size_t i = 0;
    while (name[i] && i < RAMFS_PATH_MAX - 1) {
        req->name[i] = name[i];
        i++;
    }
    req->name[i] = '\0';
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_open_reply_t *reply = (ramfs_open_reply_t *)&m;
    return reply->status == 0 ? (int64_t)reply->handle : reply->status;
}
static inline int64_t ramfs_read(uint64_t handle, void *buf, uint64_t len) {
    msg_regs_t m;
    ramfs_read_req_t *req = (ramfs_read_req_t *)&m;
    req->op = RAMFS_OP_READ;
    req->handle = handle;
    req->len = len > RAMFS_READ_MAX ? RAMFS_READ_MAX : len;
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_read_reply_t *reply = (ramfs_read_reply_t *)&m;
    if (reply->status > 0) {
        uint8_t *out = (uint8_t *)buf;
        for (int64_t i = 0; i < reply->status; i++) {
            out[i] = reply->data[i];
        }
    }
    return reply->status;
}
static inline int64_t ramfs_write(uint64_t handle, const void *buf, uint64_t len) {
    msg_regs_t m;
    ramfs_write_req_t *req = (ramfs_write_req_t *)&m;
    req->op = RAMFS_OP_WRITE;
    req->handle = handle;
    req->len = len > RAMFS_WRITE_MAX ? RAMFS_WRITE_MAX : len;
    const uint8_t *in = (const uint8_t *)buf;
    for (uint64_t i = 0; i < req->len; i++) {
        req->data[i] = in[i];
    }
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_write_reply_t *reply = (ramfs_write_reply_t *)&m;
    return reply->status;
}
static inline int64_t ramfs_close(uint64_t handle) {
    msg_regs_t m;
    ramfs_close_req_t *req = (ramfs_close_req_t *)&m;
    req->op = RAMFS_OP_CLOSE;
    req->handle = handle;
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_close_reply_t *reply = (ramfs_close_reply_t *)&m;
    return reply->status;
}
static inline int64_t ramfs_stat(const char *name, uint64_t *size_out, int *is_dir_out,
                                 uint64_t *ino_out) {
    msg_regs_t m;
    ramfs_stat_req_t *req = (ramfs_stat_req_t *)&m;
    req->op = RAMFS_OP_STAT;
    size_t i = 0;
    while (name[i] && i < RAMFS_PATH_MAX - 1) {
        req->name[i] = name[i];
        i++;
    }
    req->name[i] = '\0';
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_stat_reply_t *reply = (ramfs_stat_reply_t *)&m;
    if (reply->status == 0) {
        if (size_out) *size_out = reply->size;
        if (is_dir_out) *is_dir_out = (int)reply->is_dir;
        if (ino_out) *ino_out = reply->ino;
    }
    return reply->status;
}
static inline int64_t ramfs_fstat(uint64_t handle, uint64_t *size_out, int *is_dir_out,
                                  uint64_t *ino_out) {
    msg_regs_t m;
    ramfs_fstat_req_t *req = (ramfs_fstat_req_t *)&m;
    req->op = RAMFS_OP_FSTAT;
    req->handle = handle;
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_stat_reply_t *reply = (ramfs_stat_reply_t *)&m;
    if (reply->status == 0) {
        if (size_out) *size_out = reply->size;
        if (is_dir_out) *is_dir_out = (int)reply->is_dir;
        if (ino_out) *ino_out = reply->ino;
    }
    return reply->status;
}
static inline int64_t ramfs_readdir(uint64_t dir_ino, uint64_t index, char *name_out, int *is_dir_out) {
    msg_regs_t m;
    ramfs_readdir_req_t *req = (ramfs_readdir_req_t *)&m;
    req->op = RAMFS_OP_READDIR;
    req->dir_ino = dir_ino;
    req->index = index;
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_readdir_reply_t *reply = (ramfs_readdir_reply_t *)&m;
    if (reply->status == 0) {
        size_t i = 0;
        for (; i < RAMFS_NAME_MAX - 1 && reply->name[i]; i++) {
            name_out[i] = reply->name[i];
        }
        name_out[i] = '\0';
        if (is_dir_out) *is_dir_out = (int)reply->is_dir;
    }
    return reply->status;
}
static inline int64_t ramfs_rename(const char *oldname, const char *newname) {
    msg_regs_t m;
    ramfs_rename_req_t *req = (ramfs_rename_req_t *)&m;
    req->op = RAMFS_OP_RENAME;
    size_t i = 0;
    while (oldname[i] && i < RAMFS_NAME_MAX - 1) {
        req->oldname[i] = oldname[i];
        i++;
    }
    req->oldname[i] = '\0';
    i = 0;
    while (newname[i] && i < RAMFS_NAME_MAX - 1) {
        req->newname[i] = newname[i];
        i++;
    }
    req->newname[i] = '\0';
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_rename_reply_t *reply = (ramfs_rename_reply_t *)&m;
    return reply->status;
}
static inline int64_t ramfs_unlink(const char *name) {
    msg_regs_t m;
    ramfs_unlink_req_t *req = (ramfs_unlink_req_t *)&m;
    req->op = RAMFS_OP_UNLINK;
    size_t i = 0;
    while (name[i] && i < RAMFS_PATH_MAX - 1) {
        req->name[i] = name[i];
        i++;
    }
    req->name[i] = '\0';
    tid_t from;
    ipc_call(ramfs_server_tid(), &m, &from);
    ramfs_unlink_reply_t *reply = (ramfs_unlink_reply_t *)&m;
    return reply->status;
}
#endif
