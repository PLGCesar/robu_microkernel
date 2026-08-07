#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/devfs.h"
static int path_eq(const char *a, const char *b) {
    for (int i = 0; i < DEVFS_PATH_MAX; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == '\0') {
            return 1;
        }
    }
    return 1;
}
static int resolve_path(const char *path, dev_id_t *out) {
    if (path_eq(path, "/dev/console")) { *out = DEV_CONSOLE; return 0; }
    if (path_eq(path, "/dev/null"))    { *out = DEV_NULL;    return 0; }
    if (path_eq(path, "/dev/zero"))    { *out = DEV_ZERO;    return 0; }
    if (path_eq(path, "/dev/random"))  { *out = DEV_RANDOM;  return 0; }
    return -1;
}
static int valid_handle(uint64_t h) {
    return h == DEV_CONSOLE || h == DEV_NULL || h == DEV_ZERO || h == DEV_RANDOM;
}
#define CONSOLE_LOCAL_BUF_SIZE 64
static uint8_t console_local_buf[CONSOLE_LOCAL_BUF_SIZE];
static uint32_t console_local_head, console_local_tail;
static int rdrand_available;
static void check_rdrand(void) {
    uint32_t ecx;
    asm volatile("cpuid" : "=c"(ecx) : "a"(1) : "ebx", "edx");
    rdrand_available = (ecx >> 30) & 1;
}
static int rdrand64(uint64_t *out) {
    for (int attempt = 0; attempt < 10; attempt++) {
        uint64_t v;
        uint8_t ok;
        asm volatile("rdrand %0; setc %1" : "=r"(v), "=qm"(ok));
        if (ok) {
            *out = v;
            return 1;
        }
    }
    return 0;
}
static void handle_open(msg_regs_t *m) {
    char path[DEVFS_PATH_MAX];
    const devfs_open_req_t *req = (const devfs_open_req_t *)m;
    for (int i = 0; i < DEVFS_PATH_MAX; i++) {
        path[i] = req->path[i];
    }
    dev_id_t id;
    devfs_open_reply_t *reply = (devfs_open_reply_t *)m;
    if (resolve_path(path, &id) != 0) {
        reply->status = DEVFS_ERR_NOT_FOUND;
        return;
    }
    reply->status = 0;
    reply->handle = (uint64_t)id;
}
static void handle_read(msg_regs_t *m) {
    const devfs_read_req_t *req = (const devfs_read_req_t *)m;
    uint64_t handle = req->handle;
    uint64_t len = req->len > DEVFS_READ_MAX ? DEVFS_READ_MAX : req->len;
    devfs_read_reply_t *reply = (devfs_read_reply_t *)m;
    if (!valid_handle(handle)) {
        reply->status = DEVFS_ERR_BAD_HANDLE;
        return;
    }
    switch ((dev_id_t)handle) {
    case DEV_NULL:
        reply->status = 0;
        break;
    case DEV_ZERO:
        for (uint64_t i = 0; i < len; i++) {
            reply->data[i] = 0;
        }
        reply->status = (int64_t)len;
        break;
    case DEV_RANDOM: {
        if (!rdrand_available) {
            reply->status = DEVFS_ERR_NOT_SUPPORTED;
            break;
        }
        uint64_t got = 0;
        while (got < len) {
            uint64_t v;
            if (!rdrand64(&v)) {
                break;
            }
            uint8_t *bytes = (uint8_t *)&v;
            for (int j = 0; j < 8 && got < len; j++, got++) {
                reply->data[got] = bytes[j];
            }
        }
        reply->status = (int64_t)got;
        break;
    }
    case DEV_CONSOLE: {
        if (console_local_head == console_local_tail) {
            uint8_t kbuf[DEVFS_READ_MAX];
            int got = devfs_kernel_console_read(kbuf, sizeof(kbuf));
            if (got < 0) {
                reply->status = DEVFS_ERR_NOT_SUPPORTED;
                break;
            }
            for (int i = 0; i < got; i++) {
                uint32_t next = (console_local_head + 1) % CONSOLE_LOCAL_BUF_SIZE;
                if (next == console_local_tail) {
                    break;
                }
                console_local_buf[console_local_head] = kbuf[i];
                console_local_head = next;
            }
        }
        int n = 0;
        while ((uint64_t)n < len && console_local_head != console_local_tail) {
            reply->data[n++] = console_local_buf[console_local_tail];
            console_local_tail = (console_local_tail + 1) % CONSOLE_LOCAL_BUF_SIZE;
        }
        reply->status = n;
        break;
    }
    default:
        reply->status = DEVFS_ERR_NOT_SUPPORTED;
        break;
    }
}
static void handle_write(msg_regs_t *m) {
    const devfs_write_req_t *req = (const devfs_write_req_t *)m;
    uint64_t handle = req->handle;
    uint64_t len = req->len > DEVFS_WRITE_MAX ? DEVFS_WRITE_MAX : req->len;
    uint8_t data[DEVFS_WRITE_MAX];
    for (uint64_t i = 0; i < len; i++) {
        data[i] = req->data[i];
    }
    devfs_write_reply_t *reply = (devfs_write_reply_t *)m;
    if (!valid_handle(handle)) {
        reply->status = DEVFS_ERR_BAD_HANDLE;
        return;
    }
    switch ((dev_id_t)handle) {
    case DEV_NULL:
    case DEV_ZERO:
    case DEV_RANDOM:
        reply->status = (int64_t)len;
        break;
    case DEV_CONSOLE:
        devfs_kernel_console_write(data, len);
        reply->status = (int64_t)len;
        break;
    }
}
static void handle_close(msg_regs_t *m) {
    const devfs_close_req_t *req = (const devfs_close_req_t *)m;
    uint64_t handle = req->handle;
    devfs_close_reply_t *reply = (devfs_close_reply_t *)m;
    reply->status = valid_handle(handle) ? 0 : DEVFS_ERR_BAD_HANDLE;
}
void _start(void) {
    check_rdrand();
    msg_regs_t m;
    tid_t from;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        switch (m.word[0]) {
        case DEVFS_OP_OPEN:
            handle_open(&m);
            break;
        case DEVFS_OP_READ:
            handle_read(&m);
            break;
        case DEVFS_OP_WRITE:
            handle_write(&m);
            break;
        case DEVFS_OP_CLOSE:
            handle_close(&m);
            break;
        default:
            ((devfs_open_reply_t *)&m)->status = DEVFS_ERR_NOT_FOUND;
            break;
        }
        ipc_send(from, &m);
    }
}
