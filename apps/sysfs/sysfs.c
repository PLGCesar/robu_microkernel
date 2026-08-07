#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/ipc.h"
#include "robu/kinfo.h"
#include "robu/sysfs.h"
#define SYSFS_MAX_HANDLES 16
#define SYSFS_CONTENT_MAX 96
typedef struct {
    int in_use;
    uint64_t offset;
    uint64_t len;
    char content[SYSFS_CONTENT_MAX];
} sysfs_handle_t;
static sysfs_handle_t handles[SYSFS_MAX_HANDLES];
static int alloc_handle(void) {
    for (int i = 0; i < SYSFS_MAX_HANDLES; i++) {
        if (!handles[i].in_use) {
            return i;
        }
    }
    return -1;
}
static int valid_handle(uint64_t h) {
    return h < SYSFS_MAX_HANDLES && handles[h].in_use;
}
static int path_eq(const char *a, const char *b) {
    for (int i = 0; i < SYSFS_PATH_MAX; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == '\0') {
            return 1;
        }
    }
    return 1;
}
static int append_str(char *buf, int pos, int max, const char *s) {
    while (*s && pos < max) {
        buf[pos++] = *s++;
    }
    return pos;
}
static int append_uint(char *buf, int pos, int max, uint64_t v) {
    char digits[20];
    int nd = 0;
    if (v == 0) {
        digits[nd++] = '0';
    } else {
        while (v > 0 && nd < 20) {
            digits[nd++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    while (nd > 0 && pos < max) {
        buf[pos++] = digits[--nd];
    }
    return pos;
}
static void format_meminfo(sysfs_handle_t *hd) {
    msg_regs_t q = (msg_regs_t){0};
    q.word[0] = 0;
    robu_ipc_raw(0, 0, IPC_FLAG_SYS_INFO, &q, NULL);
    int pos = 0;
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, "total_frames=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[0]);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " free_frames=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[1]);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " alloc_calls=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[2]);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " free_calls=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[3]);
    if (pos < SYSFS_CONTENT_MAX) {
        hd->content[pos++] = '\n';
    }
    hd->len = (uint64_t)pos;
}
static void format_schedstats(sysfs_handle_t *hd) {
    msg_regs_t q = (msg_regs_t){0};
    q.word[0] = 1;
    robu_ipc_raw(0, 0, IPC_FLAG_SYS_INFO, &q, NULL);
    int pos = 0;
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, "full_scheds=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[0]);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " preempts=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[1]);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " ipc_msgs=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[2]);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " ticks=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[3]);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " timer_traps=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[4]);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " kicks_sent=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, q.word[5]);
    if (pos < SYSFS_CONTENT_MAX) {
        hd->content[pos++] = '\n';
    }
    hd->len = (uint64_t)pos;
}
static void format_uptime(sysfs_handle_t *hd) {
    const volatile kinfo_page_t *k = (const volatile kinfo_page_t *)KINFO_VA;
    uint64_t ticks = kinfo_read_ticks(k);
    int pos = 0;
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, "uptime_ticks=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, ticks);
    pos = append_str(hd->content, pos, SYSFS_CONTENT_MAX, " hz=");
    pos = append_uint(hd->content, pos, SYSFS_CONTENT_MAX, k->clock_hz);
    if (pos < SYSFS_CONTENT_MAX) {
        hd->content[pos++] = '\n';
    }
    hd->len = (uint64_t)pos;
}
static void handle_open(msg_regs_t *m) {
    const sysfs_open_req_t *req = (const sysfs_open_req_t *)m;
    char path[SYSFS_PATH_MAX];
    for (int i = 0; i < SYSFS_PATH_MAX; i++) {
        path[i] = req->path[i];
    }
    sysfs_open_reply_t *reply = (sysfs_open_reply_t *)m;
    int hidx = alloc_handle();
    if (hidx < 0) {
        reply->status = SYSFS_ERR_NO_SPACE;
        return;
    }
    sysfs_handle_t *hd = &handles[hidx];
    hd->offset = 0;
    if (path_eq(path, "meminfo")) {
        format_meminfo(hd);
    } else if (path_eq(path, "schedstats")) {
        format_schedstats(hd);
    } else if (path_eq(path, "uptime")) {
        format_uptime(hd);
    } else {
        reply->status = SYSFS_ERR_NOT_FOUND;
        return;
    }
    hd->in_use = 1;
    reply->status = 0;
    reply->handle = (uint64_t)hidx;
    reply->size = hd->len;
}
static void handle_read(msg_regs_t *m) {
    const sysfs_read_req_t *req = (const sysfs_read_req_t *)m;
    uint64_t h = req->handle;
    uint64_t len = req->len > SYSFS_READ_MAX ? SYSFS_READ_MAX : req->len;
    sysfs_read_reply_t *reply = (sysfs_read_reply_t *)m;
    if (!valid_handle(h)) {
        reply->status = SYSFS_ERR_BAD_HANDLE;
        return;
    }
    sysfs_handle_t *hd = &handles[h];
    uint64_t avail = hd->len > hd->offset ? hd->len - hd->offset : 0;
    uint64_t n = len < avail ? len : avail;
    for (uint64_t i = 0; i < n; i++) {
        reply->data[i] = (uint8_t)hd->content[hd->offset + i];
    }
    hd->offset += n;
    reply->status = (int64_t)n;
}
static void handle_close(msg_regs_t *m) {
    const sysfs_close_req_t *req = (const sysfs_close_req_t *)m;
    sysfs_close_reply_t *reply = (sysfs_close_reply_t *)m;
    uint64_t h = req->handle;
    if (!valid_handle(h)) {
        reply->status = SYSFS_ERR_BAD_HANDLE;
        return;
    }
    handles[h].in_use = 0;
    reply->status = 0;
}
void _start(void) {
    msg_regs_t m;
    tid_t from;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        switch (m.word[0]) {
        case SYSFS_OP_OPEN:
            handle_open(&m);
            break;
        case SYSFS_OP_READ:
            handle_read(&m);
            break;
        case SYSFS_OP_CLOSE:
            handle_close(&m);
            break;
        default:
            ((sysfs_open_reply_t *)&m)->status = SYSFS_ERR_NOT_FOUND;
            break;
        }
        ipc_send(from, &m);
    }
}
