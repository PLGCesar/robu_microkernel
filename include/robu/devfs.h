#ifndef ROBU_DEVFS_H
#define ROBU_DEVFS_H
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/ipc.h"
#include "robu/kinfo.h"
typedef enum {
    DEV_CONSOLE = 0,
    DEV_NULL = 1,
    DEV_ZERO = 2,
    DEV_RANDOM = 3,
} dev_id_t;
#define DEVFS_OP_OPEN  1
#define DEVFS_OP_READ  2
#define DEVFS_OP_WRITE 3
#define DEVFS_OP_CLOSE 4
#define DEVFS_ERR_NOT_FOUND    (-1)
#define DEVFS_ERR_BAD_HANDLE   (-2)
#define DEVFS_ERR_NOT_SUPPORTED (-3)
#define DEVFS_PATH_MAX 40
#define DEVFS_READ_MAX 40
#define DEVFS_WRITE_MAX 24
typedef struct {
    uint64_t op;
    char path[DEVFS_PATH_MAX];
} devfs_open_req_t;
typedef struct {
    int64_t status;
    uint64_t handle;
} devfs_open_reply_t;
typedef struct {
    uint64_t op;
    uint64_t handle;
    uint64_t len;
} devfs_read_req_t;
typedef struct {
    int64_t status;
    uint8_t data[DEVFS_READ_MAX];
} devfs_read_reply_t;
_Static_assert(sizeof(devfs_read_reply_t) == 48, "must fit one msg_regs_t");
typedef struct {
    uint64_t op;
    uint64_t handle;
    uint64_t len;
    uint8_t data[DEVFS_WRITE_MAX];
} devfs_write_req_t;
_Static_assert(sizeof(devfs_write_req_t) == 48, "must fit one msg_regs_t");
typedef struct {
    int64_t status;
} devfs_write_reply_t;
typedef struct {
    uint64_t op;
    uint64_t handle;
} devfs_close_req_t;
typedef struct {
    int64_t status;
} devfs_close_reply_t;
static inline tid_t devfs_server_tid(void) {
    return (tid_t)kinfo_user()->devfs_tid;
}
static inline int64_t devfs_open(const char *path) {
    msg_regs_t m;
    devfs_open_req_t *req = (devfs_open_req_t *)&m;
    req->op = DEVFS_OP_OPEN;
    size_t i = 0;
    while (path[i] && i < DEVFS_PATH_MAX - 1) {
        req->path[i] = path[i];
        i++;
    }
    req->path[i] = '\0';
    tid_t from;
    ipc_call(devfs_server_tid(), &m, &from);
    devfs_open_reply_t *reply = (devfs_open_reply_t *)&m;
    return reply->status == 0 ? (int64_t)reply->handle : reply->status;
}
static inline int64_t devfs_read(uint64_t handle, void *buf, uint64_t len) {
    msg_regs_t m;
    devfs_read_req_t *req = (devfs_read_req_t *)&m;
    req->op = DEVFS_OP_READ;
    req->handle = handle;
    req->len = len > DEVFS_READ_MAX ? DEVFS_READ_MAX : len;
    tid_t from;
    ipc_call(devfs_server_tid(), &m, &from);
    devfs_read_reply_t *reply = (devfs_read_reply_t *)&m;
    if (reply->status > 0) {
        uint8_t *out = (uint8_t *)buf;
        for (int64_t i = 0; i < reply->status; i++) {
            out[i] = reply->data[i];
        }
    }
    return reply->status;
}
static inline int64_t devfs_write(uint64_t handle, const void *buf, uint64_t len) {
    msg_regs_t m;
    devfs_write_req_t *req = (devfs_write_req_t *)&m;
    req->op = DEVFS_OP_WRITE;
    req->handle = handle;
    req->len = len > DEVFS_WRITE_MAX ? DEVFS_WRITE_MAX : len;
    const uint8_t *in = (const uint8_t *)buf;
    for (uint64_t i = 0; i < req->len; i++) {
        req->data[i] = in[i];
    }
    tid_t from;
    ipc_call(devfs_server_tid(), &m, &from);
    devfs_write_reply_t *reply = (devfs_write_reply_t *)&m;
    return reply->status;
}
static inline int64_t devfs_close(uint64_t handle) {
    msg_regs_t m;
    devfs_close_req_t *req = (devfs_close_req_t *)&m;
    req->op = DEVFS_OP_CLOSE;
    req->handle = handle;
    tid_t from;
    ipc_call(devfs_server_tid(), &m, &from);
    devfs_close_reply_t *reply = (devfs_close_reply_t *)&m;
    return reply->status;
}
static inline int devfs_kernel_console_write(const uint8_t *buf, uint64_t len) {
    msg_regs_t m;
    uint64_t clamped = len > 40 ? 40 : len;
    m.word[0] = clamped;
    uint64_t words[5] = {0, 0, 0, 0, 0};
    uint8_t *bytes = (uint8_t *)words;
    for (uint64_t i = 0; i < clamped; i++) {
        bytes[i] = buf[i];
    }
    m.word[1] = words[0];
    m.word[2] = words[1];
    m.word[3] = words[2];
    m.word[4] = words[3];
    m.word[5] = words[4];
    return (int)robu_ipc_raw(0, 0, IPC_FLAG_CONSOLE_WRITE, &m, NULL);
}
static inline int devfs_kernel_console_read(uint8_t *buf, uint64_t max) {
    msg_regs_t m = (msg_regs_t){0};
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_CONSOLE_READ, &m, NULL);
    if (rc != IPC_ERR_NONE) {
        return -1;
    }
    uint64_t n = m.word[0];
    if (n > max) {
        n = max;
    }
    uint64_t words[5] = { m.word[1], m.word[2], m.word[3], m.word[4], m.word[5] };
    const uint8_t *bytes = (const uint8_t *)words;
    for (uint64_t i = 0; i < n; i++) {
        buf[i] = bytes[i];
    }
    return (int)n;
}
#endif
