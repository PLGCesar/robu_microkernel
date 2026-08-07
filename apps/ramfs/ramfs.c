#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/ramfs.h"
#define RAMFS_MAX_FILES 24
#define RAMFS_MAX_HANDLES 32
#define RAMFS_MAX_DATA (144 * 1024)
typedef struct {
    int in_use;
    int is_dir;
    uint64_t parent_ino;
    char name[RAMFS_PATH_MAX];
    uint64_t size;
    uint8_t data[RAMFS_MAX_DATA];
} ramfs_file_t;
typedef struct {
    int in_use;
    int file_idx;
    uint64_t offset;
} ramfs_handle_t;
static ramfs_file_t files[RAMFS_MAX_FILES];
static ramfs_handle_t handles[RAMFS_MAX_HANDLES];
static int name_eq(const char *a, const char *b) {
    for (int i = 0; i < RAMFS_PATH_MAX; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == '\0') {
            return 1;
        }
    }
    return 1;
}
static void set_name(char *dst, const char *src, size_t dst_size) {
    size_t i = 0;
    for (; i < dst_size - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}
static int find_file(const char *name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].in_use && name_eq(files[i].name, name)) {
            return i;
        }
    }
    return -1;
}
static int alloc_file(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].in_use) {
            return i;
        }
    }
    return -1;
}
static int alloc_handle(void) {
    for (int i = 0; i < RAMFS_MAX_HANDLES; i++) {
        if (!handles[i].in_use) {
            return i;
        }
    }
    return -1;
}
static int valid_handle(uint64_t h) {
    return h < RAMFS_MAX_HANDLES && handles[h].in_use;
}
static void parent_path(const char *path, char *out, size_t out_size) {
    size_t len = 0;
    while (path[len]) {
        len++;
    }
    size_t last_slash = 0;
    int found = 0;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            last_slash = i;
            found = 1;
        }
    }
    if (!found) {
        out[0] = '\0';
        return;
    }
    size_t n = last_slash < out_size - 1 ? last_slash : out_size - 1;
    for (size_t i = 0; i < n; i++) {
        out[i] = path[i];
    }
    out[n] = '\0';
}
static uint64_t resolve_parent_ino(const char *path) {
    char parent[RAMFS_PATH_MAX];
    parent_path(path, parent, sizeof(parent));
    if (parent[0] == '\0') {
        return RAMFS_ROOT_INO;
    }
    int idx = find_file(parent);
    if (idx < 0) {
        return RAMFS_ROOT_INO;
    }
    return (uint64_t)idx + 2;
}
static const char *basename_of(const char *path) {
    size_t len = 0;
    while (path[len]) {
        len++;
    }
    size_t last_slash = 0;
    int found = 0;
    for (size_t i = 0; i < len; i++) {
        if (path[i] == '/') {
            last_slash = i;
            found = 1;
        }
    }
    return found ? path + last_slash + 1 : path;
}
static void handle_open(msg_regs_t *m) {
    char name[RAMFS_PATH_MAX];
    const ramfs_open_req_t *req = (const ramfs_open_req_t *)m;
    uint64_t flags = req->flags;
    for (int i = 0; i < RAMFS_PATH_MAX; i++) {
        name[i] = req->name[i];
    }
    ramfs_open_reply_t *reply = (ramfs_open_reply_t *)m;
    int fidx = find_file(name);
    if (fidx >= 0 && files[fidx].is_dir) {
        reply->status = RAMFS_ERR_IS_DIR;
        return;
    }
    if (fidx < 0) {
        if (!(flags & RAMFS_O_CREAT)) {
            reply->status = RAMFS_ERR_NOT_FOUND;
            return;
        }
        fidx = alloc_file();
        if (fidx < 0) {
            reply->status = RAMFS_ERR_NO_SPACE;
            return;
        }
        files[fidx].in_use = 1;
        files[fidx].is_dir = 0;
        files[fidx].size = 0;
        set_name(files[fidx].name, name, sizeof(files[fidx].name));
        files[fidx].parent_ino = resolve_parent_ino(name);
    } else if (flags & RAMFS_O_TRUNC) {
        files[fidx].size = 0;
    }
    int hidx = alloc_handle();
    if (hidx < 0) {
        reply->status = RAMFS_ERR_NO_SPACE;
        return;
    }
    handles[hidx].in_use = 1;
    handles[hidx].file_idx = fidx;
    handles[hidx].offset = (flags & RAMFS_O_APPEND) ? files[fidx].size : 0;
    reply->status = 0;
    reply->handle = (uint64_t)hidx;
}
static void handle_read(msg_regs_t *m) {
    const ramfs_read_req_t *req = (const ramfs_read_req_t *)m;
    uint64_t h = req->handle;
    uint64_t len = req->len > RAMFS_READ_MAX ? RAMFS_READ_MAX : req->len;
    ramfs_read_reply_t *reply = (ramfs_read_reply_t *)m;
    if (!valid_handle(h)) {
        reply->status = RAMFS_ERR_BAD_HANDLE;
        return;
    }
    ramfs_handle_t *hd = &handles[h];
    ramfs_file_t *f = &files[hd->file_idx];
    uint64_t avail = f->size > hd->offset ? f->size - hd->offset : 0;
    uint64_t n = len < avail ? len : avail;
    for (uint64_t i = 0; i < n; i++) {
        reply->data[i] = f->data[hd->offset + i];
    }
    hd->offset += n;
    reply->status = (int64_t)n;
}
static void handle_write(msg_regs_t *m) {
    const ramfs_write_req_t *req = (const ramfs_write_req_t *)m;
    uint64_t h = req->handle;
    uint64_t len = req->len > RAMFS_WRITE_MAX ? RAMFS_WRITE_MAX : req->len;
    uint8_t data[RAMFS_WRITE_MAX];
    for (uint64_t i = 0; i < len; i++) {
        data[i] = req->data[i];
    }
    ramfs_write_reply_t *reply = (ramfs_write_reply_t *)m;
    if (!valid_handle(h)) {
        reply->status = RAMFS_ERR_BAD_HANDLE;
        return;
    }
    ramfs_handle_t *hd = &handles[h];
    ramfs_file_t *f = &files[hd->file_idx];
    uint64_t space = RAMFS_MAX_DATA > hd->offset ? RAMFS_MAX_DATA - hd->offset : 0;
    uint64_t n = len < space ? len : space;
    for (uint64_t i = 0; i < n; i++) {
        f->data[hd->offset + i] = data[i];
    }
    hd->offset += n;
    if (hd->offset > f->size) {
        f->size = hd->offset;
    }
    reply->status = (int64_t)n;
}
static void handle_close(msg_regs_t *m) {
    const ramfs_close_req_t *req = (const ramfs_close_req_t *)m;
    ramfs_close_reply_t *reply = (ramfs_close_reply_t *)m;
    uint64_t h = req->handle;
    if (!valid_handle(h)) {
        reply->status = RAMFS_ERR_BAD_HANDLE;
        return;
    }
    handles[h].in_use = 0;
    reply->status = 0;
}
static void handle_stat(msg_regs_t *m) {
    char name[RAMFS_PATH_MAX];
    const ramfs_stat_req_t *req = (const ramfs_stat_req_t *)m;
    for (int i = 0; i < RAMFS_PATH_MAX; i++) {
        name[i] = req->name[i];
    }
    ramfs_stat_reply_t *reply = (ramfs_stat_reply_t *)m;
    if (name[0] == '/' && name[1] == '\0') {
        reply->status = 0;
        reply->size = 0;
        reply->is_dir = 1;
        reply->ino = RAMFS_ROOT_INO;
        return;
    }
    int fidx = find_file(name);
    if (fidx < 0) {
        reply->status = RAMFS_ERR_NOT_FOUND;
        return;
    }
    reply->status = 0;
    reply->size = files[fidx].size;
    reply->is_dir = (uint64_t)files[fidx].is_dir;
    reply->ino = (uint64_t)fidx + 2;
}
static void handle_fstat(msg_regs_t *m) {
    const ramfs_fstat_req_t *req = (const ramfs_fstat_req_t *)m;
    uint64_t h = req->handle;
    ramfs_stat_reply_t *reply = (ramfs_stat_reply_t *)m;
    if (!valid_handle(h)) {
        reply->status = RAMFS_ERR_BAD_HANDLE;
        return;
    }
    reply->status = 0;
    reply->size = files[handles[h].file_idx].size;
    reply->is_dir = (uint64_t)files[handles[h].file_idx].is_dir;
    reply->ino = (uint64_t)handles[h].file_idx + 2;
}
static void handle_readdir(msg_regs_t *m) {
    const ramfs_readdir_req_t *req = (const ramfs_readdir_req_t *)m;
    uint64_t dir_ino = req->dir_ino;
    uint64_t want = req->index;
    ramfs_readdir_reply_t *reply = (ramfs_readdir_reply_t *)m;
    uint64_t seen = 0;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].in_use || files[i].parent_ino != dir_ino) {
            continue;
        }
        if (seen == want) {
            reply->status = 0;
            reply->is_dir = (uint64_t)files[i].is_dir;
            set_name(reply->name, basename_of(files[i].name), sizeof(reply->name));
            return;
        }
        seen++;
    }
    reply->status = RAMFS_ERR_NOT_FOUND;
}
static void handle_rename(msg_regs_t *m) {
    char oldname[RAMFS_NAME_MAX], newname[RAMFS_NAME_MAX];
    const ramfs_rename_req_t *req = (const ramfs_rename_req_t *)m;
    for (int i = 0; i < RAMFS_NAME_MAX; i++) {
        oldname[i] = req->oldname[i];
        newname[i] = req->newname[i];
    }
    ramfs_rename_reply_t *reply = (ramfs_rename_reply_t *)m;
    int fidx = find_file(oldname);
    if (fidx < 0) {
        reply->status = RAMFS_ERR_NOT_FOUND;
        return;
    }
    if (files[fidx].is_dir) {
        reply->status = RAMFS_ERR_IS_DIR;
        return;
    }
    int existing = find_file(newname);
    if (existing >= 0) {
        if (files[existing].is_dir) {
            reply->status = RAMFS_ERR_IS_DIR;
            return;
        }
        if (existing != fidx) {
            files[existing].in_use = 0;
        }
    }
    set_name(files[fidx].name, newname, sizeof(files[fidx].name));
    files[fidx].parent_ino = resolve_parent_ino(newname);
    reply->status = 0;
}
static void handle_unlink(msg_regs_t *m) {
    char name[RAMFS_PATH_MAX];
    const ramfs_unlink_req_t *req = (const ramfs_unlink_req_t *)m;
    for (int i = 0; i < RAMFS_PATH_MAX; i++) {
        name[i] = req->name[i];
    }
    ramfs_unlink_reply_t *reply = (ramfs_unlink_reply_t *)m;
    int fidx = find_file(name);
    if (fidx < 0) {
        reply->status = RAMFS_ERR_NOT_FOUND;
        return;
    }
    if (files[fidx].is_dir) {
        reply->status = RAMFS_ERR_IS_DIR;
        return;
    }
    files[fidx].in_use = 0;
    reply->status = 0;
}
static uint64_t seed_dir(const char *name, uint64_t parent_ino) {
    int idx = alloc_file();
    if (idx < 0) {
        return 0;
    }
    files[idx].in_use = 1;
    files[idx].is_dir = 1;
    files[idx].parent_ino = parent_ino;
    files[idx].size = 0;
    set_name(files[idx].name, name, sizeof(files[idx].name));
    return (uint64_t)idx + 2;
}
static void seed_fixed_dirs(void) {
    seed_dir("bin", RAMFS_ROOT_INO);
    seed_dir("etc", RAMFS_ROOT_INO);
    uint64_t var_ino = seed_dir("var", RAMFS_ROOT_INO);
    if (var_ino) {
        seed_dir("var/tmp", var_ino);
    }
}
void _start(void) {
    msg_regs_t m;
    tid_t from;
    seed_fixed_dirs();
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        switch (m.word[0]) {
        case RAMFS_OP_OPEN:
            handle_open(&m);
            break;
        case RAMFS_OP_READ:
            handle_read(&m);
            break;
        case RAMFS_OP_WRITE:
            handle_write(&m);
            break;
        case RAMFS_OP_CLOSE:
            handle_close(&m);
            break;
        case RAMFS_OP_STAT:
            handle_stat(&m);
            break;
        case RAMFS_OP_FSTAT:
            handle_fstat(&m);
            break;
        case RAMFS_OP_READDIR:
            handle_readdir(&m);
            break;
        case RAMFS_OP_RENAME:
            handle_rename(&m);
            break;
        case RAMFS_OP_UNLINK:
            handle_unlink(&m);
            break;
        default:
            ((ramfs_open_reply_t *)&m)->status = RAMFS_ERR_NOT_FOUND;
            break;
        }
        ipc_send(from, &m);
    }
}
