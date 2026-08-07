#ifndef ROBU_SYSFS_H
#define ROBU_SYSFS_H
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#define SYSFS_OP_OPEN  1
#define SYSFS_OP_READ  2
#define SYSFS_OP_CLOSE 3
#define SYSFS_ERR_NOT_FOUND  (-1)
#define SYSFS_ERR_BAD_HANDLE (-2)
#define SYSFS_ERR_NO_SPACE   (-3)
#define SYSFS_PATH_MAX 16
#define SYSFS_READ_MAX 40
typedef struct {
    uint64_t op;
    char path[SYSFS_PATH_MAX];
} sysfs_open_req_t;
typedef struct {
    int64_t status;
    uint64_t handle;
    uint64_t size;
} sysfs_open_reply_t;
typedef struct {
    uint64_t op;
    uint64_t handle;
    uint64_t len;
} sysfs_read_req_t;
typedef struct {
    int64_t status;
    uint8_t data[SYSFS_READ_MAX];
} sysfs_read_reply_t;
_Static_assert(sizeof(sysfs_read_reply_t) <= 48, "must fit one msg_regs_t");
typedef struct {
    uint64_t op;
    uint64_t handle;
} sysfs_close_req_t;
typedef struct {
    int64_t status;
} sysfs_close_reply_t;
static inline tid_t sysfs_server_tid(void) {
    return (tid_t)kinfo_user()->sysfs_tid;
}
static inline int64_t sysfs_open(const char *path, uint64_t *size_out) {
    msg_regs_t m;
    sysfs_open_req_t *req = (sysfs_open_req_t *)&m;
    req->op = SYSFS_OP_OPEN;
    size_t i = 0;
    while (path[i] && i < SYSFS_PATH_MAX - 1) {
        req->path[i] = path[i];
        i++;
    }
    req->path[i] = '\0';
    tid_t from;
    ipc_call(sysfs_server_tid(), &m, &from);
    sysfs_open_reply_t *reply = (sysfs_open_reply_t *)&m;
    if (reply->status == 0 && size_out) {
        *size_out = reply->size;
    }
    return reply->status == 0 ? (int64_t)reply->handle : reply->status;
}
static inline int64_t sysfs_read(uint64_t handle, void *buf, uint64_t len) {
    msg_regs_t m;
    sysfs_read_req_t *req = (sysfs_read_req_t *)&m;
    req->op = SYSFS_OP_READ;
    req->handle = handle;
    req->len = len > SYSFS_READ_MAX ? SYSFS_READ_MAX : len;
    tid_t from;
    ipc_call(sysfs_server_tid(), &m, &from);
    sysfs_read_reply_t *reply = (sysfs_read_reply_t *)&m;
    if (reply->status > 0) {
        uint8_t *out = (uint8_t *)buf;
        for (int64_t i = 0; i < reply->status; i++) {
            out[i] = reply->data[i];
        }
    }
    return reply->status;
}
static inline int64_t sysfs_close(uint64_t handle) {
    msg_regs_t m;
    sysfs_close_req_t *req = (sysfs_close_req_t *)&m;
    req->op = SYSFS_OP_CLOSE;
    req->handle = handle;
    tid_t from;
    ipc_call(sysfs_server_tid(), &m, &from);
    sysfs_close_reply_t *reply = (sysfs_close_reply_t *)&m;
    return reply->status;
}
#endif
