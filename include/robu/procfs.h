#ifndef ROBU_PROCFS_H
#define ROBU_PROCFS_H
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#define PROCFS_OP_OPEN  1
#define PROCFS_OP_READ  2
#define PROCFS_OP_CLOSE 3
#define PROCFS_ERR_NOT_FOUND  (-1)
#define PROCFS_ERR_BAD_HANDLE (-2)
#define PROCFS_ERR_NO_SPACE   (-3)
#define PROCFS_PATH_MAX 24
#define PROCFS_READ_MAX 40
typedef struct {
    uint64_t op;
    char path[PROCFS_PATH_MAX];
} procfs_open_req_t;
typedef struct {
    int64_t status;
    uint64_t handle;
    uint64_t size;
} procfs_open_reply_t;
typedef struct {
    uint64_t op;
    uint64_t handle;
    uint64_t len;
} procfs_read_req_t;
typedef struct {
    int64_t status;
    uint8_t data[PROCFS_READ_MAX];
} procfs_read_reply_t;
_Static_assert(sizeof(procfs_read_reply_t) <= 48, "must fit one msg_regs_t");
typedef struct {
    uint64_t op;
    uint64_t handle;
} procfs_close_req_t;
typedef struct {
    int64_t status;
} procfs_close_reply_t;
static inline tid_t procfs_server_tid(void) {
    return (tid_t)kinfo_user()->procfs_tid;
}
static inline int64_t procfs_open(const char *path, uint64_t *size_out) {
    msg_regs_t m;
    procfs_open_req_t *req = (procfs_open_req_t *)&m;
    req->op = PROCFS_OP_OPEN;
    size_t i = 0;
    while (path[i] && i < PROCFS_PATH_MAX - 1) {
        req->path[i] = path[i];
        i++;
    }
    req->path[i] = '\0';
    tid_t from;
    ipc_call(procfs_server_tid(), &m, &from);
    procfs_open_reply_t *reply = (procfs_open_reply_t *)&m;
    if (reply->status == 0 && size_out) {
        *size_out = reply->size;
    }
    return reply->status == 0 ? (int64_t)reply->handle : reply->status;
}
static inline int64_t procfs_read(uint64_t handle, void *buf, uint64_t len) {
    msg_regs_t m;
    procfs_read_req_t *req = (procfs_read_req_t *)&m;
    req->op = PROCFS_OP_READ;
    req->handle = handle;
    req->len = len > PROCFS_READ_MAX ? PROCFS_READ_MAX : len;
    tid_t from;
    ipc_call(procfs_server_tid(), &m, &from);
    procfs_read_reply_t *reply = (procfs_read_reply_t *)&m;
    if (reply->status > 0) {
        uint8_t *out = (uint8_t *)buf;
        for (int64_t i = 0; i < reply->status; i++) {
            out[i] = reply->data[i];
        }
    }
    return reply->status;
}
static inline int64_t procfs_close(uint64_t handle) {
    msg_regs_t m;
    procfs_close_req_t *req = (procfs_close_req_t *)&m;
    req->op = PROCFS_OP_CLOSE;
    req->handle = handle;
    tid_t from;
    ipc_call(procfs_server_tid(), &m, &from);
    procfs_close_reply_t *reply = (procfs_close_reply_t *)&m;
    return reply->status;
}
#endif
