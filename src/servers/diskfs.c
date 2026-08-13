#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/ipc.h"
#include "robu/vfs.h"

static void *ext2_memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while(n--) *p++ = (uint8_t)c;
    return s;
}

static void *ext2_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while(n--) *d++ = *s++;
    return dest;
}

static size_t ext2_strlen(const char *s) {
    size_t l = 0;
    while(s[l]) l++;
    return l;
}

static int ext2_strncmp(const char *a, const char *b, size_t n) {
    while (n--) {
        if (*a != *b) return (uint8_t)*a - (uint8_t)*b;
        if (!*a) break;
        a++; b++;
    }
    return 0;
}

/* Ext2 Structures */
struct ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
} __attribute__((packed));

struct ext2_block_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed));

struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint32_t i_osd2[3];
} __attribute__((packed));

struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[];
} __attribute__((packed));

static uint32_t ext2_inodes_count;
static uint32_t ext2_blocks_count;
static uint32_t ext2_block_size;
static uint32_t ext2_inodes_per_group;
static uint32_t ext2_first_data_block;
static uint16_t ext2_inode_size;
static uint32_t ext2_blocks_per_pointer;

/* Kernel Block I/O Layer Interfacing */
static int ext2_read_sectors(uint64_t sector, uint32_t count, void *buf) {
    uint8_t *p = (uint8_t *)buf;
    while (count > 0) {
        uint32_t n = count > 8 ? 8 : count;
        msg_regs_t m = {0};
        m.word[0] = 0; // cmd_load
        m.word[1] = sector;
        m.word[2] = n;
        if (robu_ipc_raw(0, 0, IPC_FLAG_BLK_IO, &m, NULL) != 0) return -1;

        uint32_t bytes = n * 512;
        uint32_t off = 0;
        while (off < bytes) {
            m = (msg_regs_t){0};
            m.word[0] = 1; // cmd_read_chunk
            m.word[1] = off;
            int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_BLK_IO, &m, NULL);
            if (rc < 0) return -1;
            
            uint32_t chunk = (uint32_t)rc;
            if (chunk == 0) break;
            
            uint64_t words[5] = { m.word[0], m.word[1], m.word[2], m.word[3], m.word[4] };
            uint8_t *src = (uint8_t *)words;
            for (uint32_t i = 0; i < chunk; i++) {
                if (off + i < bytes) p[off + i] = src[i];
            }
            off += chunk;
        }
        sector += n;
        count -= n;
        p += bytes;
    }
    return 0;
}

/* Local Single-Block cache pra reduzir Context Switches do Microkernel (Super veloz) */
static uint32_t cached_block = 0xFFFFFFFF;
static uint8_t cached_buf[4096];

static int ext2_read_block(uint32_t block, void *buf) {
    if (block == cached_block) {
        ext2_memcpy(buf, cached_buf, ext2_block_size);
        return 0;
    }
    uint64_t sector = (uint64_t)block * (ext2_block_size / 512);
    if (ext2_read_sectors(sector, ext2_block_size / 512, cached_buf) != 0) return -1;
    cached_block = block;
    ext2_memcpy(buf, cached_buf, ext2_block_size);
    return 0;
}

/* Ext2 Boot & Lookup Functions */
static int ext2_mount(void) {
    uint8_t buf[1024];
    if (ext2_read_sectors(2, 2, buf) != 0) return -1;
    struct ext2_superblock *sb = (struct ext2_superblock *)buf;
    if (sb->s_magic != 0xEF53) return -1;

    ext2_inodes_count = sb->s_inodes_count;
    ext2_blocks_count = sb->s_blocks_count;
    ext2_block_size = 1024 << sb->s_log_block_size;
    ext2_inodes_per_group = sb->s_inodes_per_group;
    ext2_first_data_block = sb->s_first_data_block;
    ext2_inode_size = (sb->s_rev_level >= 1) ? sb->s_inode_size : 128;
    ext2_blocks_per_pointer = ext2_block_size / 4;
    return 0;
}

static struct ext2_block_group_desc ext2_get_bgd(uint32_t bg) {
    uint32_t bgd_block = ext2_first_data_block + 1;
    uint32_t bgd_offset = bg * sizeof(struct ext2_block_group_desc);
    uint32_t b = bgd_block + (bgd_offset / ext2_block_size);
    uint32_t off = bgd_offset % ext2_block_size;
    
    uint8_t buf[4096];
    ext2_read_block(b, buf);
    
    struct ext2_block_group_desc bgd;
    ext2_memcpy(&bgd, buf + off, sizeof(bgd));
    return bgd;
}

static int ext2_read_inode(uint32_t ino, struct ext2_inode *inode) {
    if (ino == 0) return -1;
    uint32_t bg = (ino - 1) / ext2_inodes_per_group;
    uint32_t index = (ino - 1) % ext2_inodes_per_group;
    struct ext2_block_group_desc bgd = ext2_get_bgd(bg);
    uint32_t table_block = bgd.bg_inode_table;
    uint32_t block_offset = (index * ext2_inode_size) / ext2_block_size;
    uint32_t offset_in_block = (index * ext2_inode_size) % ext2_block_size;

    uint8_t buf[4096];
    if (ext2_read_block(table_block + block_offset, buf) != 0) return -1;
    ext2_memcpy(inode, buf + offset_in_block, sizeof(struct ext2_inode));
    return 0;
}

static uint32_t ext2_get_block(struct ext2_inode *ino, uint32_t file_block) {
    uint32_t p = ext2_blocks_per_pointer;
    if (file_block < 12) return ino->i_block[file_block];
    file_block -= 12;
    if (file_block < p) {
        uint32_t ind = ino->i_block[12];
        if (!ind) return 0;
        uint32_t buf[1024];
        if (ext2_read_block(ind, buf) != 0) return 0;
        return buf[file_block];
    }
    file_block -= p;
    if (file_block < p * p) {
        uint32_t dind = ino->i_block[13];
        if (!dind) return 0;
        uint32_t buf[1024];
        if (ext2_read_block(dind, buf) != 0) return 0;
        uint32_t ind = buf[file_block / p];
        if (!ind) return 0;
        if (ext2_read_block(ind, buf) != 0) return 0;
        return buf[file_block % p];
    }
    file_block -= p * p;
    uint32_t tind = ino->i_block[14];
    if (!tind) return 0;
    uint32_t buf[1024];
    if (ext2_read_block(tind, buf) != 0) return 0;
    uint32_t dind = buf[file_block / (p * p)];
    if (!dind) return 0;
    if (ext2_read_block(dind, buf) != 0) return 0;
    uint32_t ind = buf[(file_block / p) % p];
    if (!ind) return 0;
    if (ext2_read_block(ind, buf) != 0) return 0;
    return buf[file_block % p];
}

static int ext2_lookup(uint32_t dir_ino, const char *name, uint32_t *out_ino) {
    struct ext2_inode dir;
    if (ext2_read_inode(dir_ino, &dir) != 0) return -1;
    if (!(dir.i_mode & 0x4000)) return -1; 

    uint32_t blocks = (dir.i_size + ext2_block_size - 1) / ext2_block_size;
    uint8_t buf[4096];

    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t b = ext2_get_block(&dir, i);
        if (!b) continue;
        if (ext2_read_block(b, buf) != 0) continue;

        uint32_t off = 0;
        while (off < ext2_block_size) {
            struct ext2_dir_entry *ent = (struct ext2_dir_entry *)(buf + off);
            if (ent->rec_len == 0) break;
            if (ent->inode != 0) {
                size_t len = ent->name_len;
                size_t namelen = ext2_strlen(name);
                if (len == namelen && ext2_strncmp(ent->name, name, len) == 0) {
                    *out_ino = ent->inode;
                    return 0;
                }
            }
            off += ent->rec_len;
        }
    }
    return -1;
}

static int ext2_resolve_path_internal(const char *path, uint32_t start_ino, uint32_t *out_ino, int *hops) {
    uint32_t ino = start_ino;
    if (*path == '/') {
        ino = 2; // Root Inode
        while (*path == '/') path++;
    }
    if (*path == '\0') {
        *out_ino = ino;
        return 0;
    }

    char comp[256];
    while (*path) {
        while (*path == '/') path++;
        if (!*path) break;

        int i = 0;
        while (*path && *path != '/' && i < 255) {
            comp[i++] = *path++;
        }
        comp[i] = '\0';

        uint32_t parent_ino = ino;
        uint32_t next_ino;
        if (ext2_lookup(ino, comp, &next_ino) != 0) {
            return -1;
        }
        ino = next_ino;

        struct ext2_inode inode;
        if (ext2_read_inode(ino, &inode) != 0) return -1;

        /* Symlink handling */
        if ((inode.i_mode & 0xF000) == 0xA000) {
            if (++(*hops) > 8) return -1;
            char link_path[1024];
            if (inode.i_size >= sizeof(link_path)) return -1;
            if (inode.i_size <= 60) {
                ext2_memcpy(link_path, inode.i_block, inode.i_size);
                link_path[inode.i_size] = '\0';
            } else {
                uint8_t buf[4096];
                if (ext2_read_block(ext2_get_block(&inode, 0), buf) != 0) return -1;
                ext2_memcpy(link_path, buf, inode.i_size);
                link_path[inode.i_size] = '\0';
            }

            uint32_t resolved_link_ino;
            if (ext2_resolve_path_internal(link_path, parent_ino, &resolved_link_ino, hops) != 0) {
                return -1;
            }
            ino = resolved_link_ino;
        }
    }
    *out_ino = ino;
    return 0;
}

static int ext2_resolve_path(const char *path, uint32_t *out_ino) {
    int hops = 0;
    return ext2_resolve_path_internal(path, 2, out_ino, &hops);
}

/* VFS Handlers & Desk Descriptors */
#define MAX_HANDLES 32
typedef struct {
    int in_use;
    uint32_t inode_num;
    uint64_t offset;
    uint64_t size;
    int is_dir;
} handle_t;
static handle_t handles[MAX_HANDLES];

static int alloc_handle(void) {
    for (int i = 0; i < MAX_HANDLES; i++) {
        if (!handles[i].in_use) return i;
    }
    return -1;
}

static int valid_handle(uint64_t h) {
    return h < MAX_HANDLES && handles[h].in_use;
}

static void handle_open(msg_regs_t *m) {
    vfs_open_req_t *req = (vfs_open_req_t *)m;
    vfs_open_reply_t *reply = (vfs_open_reply_t *)m;

    char path[VFS_PATH_MAX];
    for (int i = 0; i < VFS_PATH_MAX; i++) path[i] = req->name[i];

    if ((req->flags & VFS_O_CREAT) || (req->flags & VFS_O_TRUNC)) {
        reply->status = VFS_ERR_NOT_SUPPORTED;
        return;
    }

    uint32_t ino;
    if (ext2_resolve_path(path, &ino) != 0) {
        reply->status = VFS_ERR_NOT_FOUND;
        return;
    }

    struct ext2_inode inode;
    if (ext2_read_inode(ino, &inode) != 0) {
        reply->status = VFS_ERR_NOT_FOUND;
        return;
    }

    int is_dir = (inode.i_mode & 0x4000) != 0;

    int h = alloc_handle();
    if (h < 0) {
        reply->status = VFS_ERR_NO_SPACE;
        return;
    }
    handles[h].in_use = 1;
    handles[h].inode_num = ino;
    handles[h].offset = 0;
    handles[h].size = inode.i_size;
    handles[h].is_dir = is_dir;

    reply->status = 0;
    reply->handle = (uint64_t)h;
}

static void handle_read(msg_regs_t *m) {
    vfs_read_req_t *req = (vfs_read_req_t *)m;
    vfs_read_reply_t *reply = (vfs_read_reply_t *)m;
    uint64_t h = req->handle;
    if (!valid_handle(h)) {
        reply->status = VFS_ERR_BAD_HANDLE;
        return;
    }
    handle_t *hd = &handles[h];
    if (hd->is_dir) {
        reply->status = VFS_ERR_IS_DIR;
        return;
    }

    struct ext2_inode inode;
    if (ext2_read_inode(hd->inode_num, &inode) != 0) {
        reply->status = VFS_ERR_NOT_FOUND;
        return;
    }

    uint64_t len = req->len > VFS_READ_MAX ? VFS_READ_MAX : req->len;
    uint64_t avail = hd->size > hd->offset ? hd->size - hd->offset : 0;
    uint64_t n = len < avail ? len : avail;
    uint64_t got = 0;
    uint8_t blockbuf[4096];

    while (got < n) {
        uint64_t file_off = hd->offset + got;
        uint32_t file_block = file_off / ext2_block_size;
        uint32_t block_off = file_off % ext2_block_size;

        uint32_t phys_block = ext2_get_block(&inode, file_block);
        if (phys_block) {
            if (ext2_read_block(phys_block, blockbuf) != 0) break;
        } else {
            ext2_memset(blockbuf, 0, ext2_block_size);
        }

        uint64_t chunk = ext2_block_size - block_off;
        if (chunk > n - got) chunk = n - got;
        ext2_memcpy(reply->data + got, blockbuf + block_off, chunk);
        got += chunk;
    }

    hd->offset += got;
    reply->status = (int64_t)got;
}

static void handle_close(msg_regs_t *m) {
    vfs_close_req_t *req = (vfs_close_req_t *)m;
    vfs_close_reply_t *reply = (vfs_close_reply_t *)m;
    if (!valid_handle(req->handle)) {
        reply->status = VFS_ERR_BAD_HANDLE;
        return;
    }
    handles[req->handle].in_use = 0;
    reply->status = 0;
}

static void handle_stat(msg_regs_t *m) {
    vfs_stat_req_t *req = (vfs_stat_req_t *)m;
    vfs_stat_reply_t *reply = (vfs_stat_reply_t *)m;
    char path[VFS_PATH_MAX];
    for (int i = 0; i < VFS_PATH_MAX; i++) path[i] = req->name[i];

    uint32_t ino;
    if (ext2_resolve_path(path, &ino) != 0) {
        reply->status = VFS_ERR_NOT_FOUND;
        return;
    }

    struct ext2_inode inode;
    if (ext2_read_inode(ino, &inode) != 0) {
        reply->status = VFS_ERR_NOT_FOUND;
        return;
    }

    reply->status = 0;
    reply->size = inode.i_size;
    reply->is_dir = (inode.i_mode & 0x4000) != 0;
    reply->ino = ino;
}

static void handle_fstat(msg_regs_t *m) {
    vfs_fstat_req_t *req = (vfs_fstat_req_t *)m;
    vfs_stat_reply_t *reply = (vfs_stat_reply_t *)m;
    uint64_t h = req->handle;
    if (!valid_handle(h)) {
        reply->status = VFS_ERR_BAD_HANDLE;
        return;
    }
    reply->status = 0;
    reply->size = handles[h].size;
    reply->is_dir = handles[h].is_dir;
    reply->ino = handles[h].inode_num;
}

static void handle_readdir(msg_regs_t *m) {
    vfs_readdir_req_t *req = (vfs_readdir_req_t *)m;
    vfs_readdir_reply_t *reply = (vfs_readdir_reply_t *)m;
    
    struct ext2_inode dir;
    if (ext2_read_inode(req->dir_ino, &dir) != 0 || !(dir.i_mode & 0x4000)) {
        reply->status = VFS_ERR_NOT_DIR;
        return;
    }

    uint32_t blocks = (dir.i_size + ext2_block_size - 1) / ext2_block_size;
    uint8_t buf[4096];
    uint64_t seen = 0;

    for (uint32_t i = 0; i < blocks; i++) {
        uint32_t b = ext2_get_block(&dir, i);
        if (!b) continue;
        if (ext2_read_block(b, buf) != 0) continue;

        uint32_t off = 0;
        while (off < ext2_block_size) {
            struct ext2_dir_entry *ent = (struct ext2_dir_entry *)(buf + off);
            if (ent->rec_len == 0) break;
            if (ent->inode != 0) {
                if (seen == req->index) {
                    reply->status = 0;
                    struct ext2_inode target_in;
                    if (ext2_read_inode(ent->inode, &target_in) == 0) {
                        reply->is_dir = (target_in.i_mode & 0x4000) ? 1 : 0;
                    } else {
                        reply->is_dir = 0;
                    }
                    size_t copy_len = ent->name_len;
                    if (copy_len >= VFS_NAME_MAX) copy_len = VFS_NAME_MAX - 1;
                    ext2_memcpy(reply->name, ent->name, copy_len);
                    reply->name[copy_len] = '\0';
                    return;
                }
                seen++;
            }
            off += ent->rec_len;
        }
    }
    reply->status = VFS_ERR_NOT_FOUND;
}

void _start(void) {
    ext2_mount();
    msg_regs_t m;
    tid_t from;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        switch (m.word[0]) {
        case VFS_OP_OPEN:
            handle_open(&m);
            break;
        case VFS_OP_READ:
            handle_read(&m);
            break;
        case VFS_OP_WRITE:
            m.word[0] = (uint64_t)(int64_t)VFS_ERR_NOT_SUPPORTED;
            break;
        case VFS_OP_CLOSE:
            handle_close(&m);
            break;
        case VFS_OP_STAT:
            handle_stat(&m);
            break;
        case VFS_OP_FSTAT:
            handle_fstat(&m);
            break;
        case VFS_OP_READDIR:
            handle_readdir(&m);
            break;
        case VFS_OP_QUIESCE:
            m.word[0] = 0; // Read-only, nada para fechar
            break;
        case VFS_OP_RENAME:
        case VFS_OP_UNLINK:
        case VFS_OP_SYMLINK:
        case VFS_OP_MKDIR:
        case VFS_OP_RMDIR:
        case VFS_OP_LINK:
        case VFS_OP_MKNOD:
            m.word[0] = (uint64_t)(int64_t)VFS_ERR_NOT_SUPPORTED;
            break;
        default:
            m.word[0] = (uint64_t)(int64_t)VFS_ERR_NOT_FOUND;
            break;
        }
        ipc_send(from, &m);
    }
}
