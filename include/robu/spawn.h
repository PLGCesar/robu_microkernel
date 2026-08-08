#ifndef ROBU_SPAWN_H
#define ROBU_SPAWN_H
#include "robu/types.h"
#include "robu/tcb.h"
void spawn_exit_current(int status);
#define SPAWN_REQ_MAGIC   0x314e5053u
#define SPAWN_MAX_ARGS    64
#define SPAWN_REQ_MAX_LEN 4096
typedef struct {
    uint32_t off;
    uint32_t len;
} robu_spawn_str_t;
typedef struct {
    uint32_t magic;
    uint32_t total_len;
    uint32_t name_off, name_len;
    uint32_t argc, argv_off;
    uint32_t envc, envp_off;
    uint32_t nfds, fds_off;
} robu_spawn_req_t;
typedef struct {
    uint32_t fd;
    uint32_t kind;
    uint64_t handle;
    uint32_t server_tid; /* FD_VFS/FD_VFS_DIR only (sysdeps.cpp); opaque to
                             the kernel like `handle`, just copied through. */
} robu_spawn_fd_t;
#define SPAWN_FD_KIND_PIPE_READ  100
#define SPAWN_FD_KIND_PIPE_WRITE 101
#define SPAWN_FD_INFO_MAX 3
#define SPAWN_INFO_MAGIC 0x314e4953u
typedef struct {
    uint32_t magic;
    uint32_t nfds;
} robu_spawn_info_t;
int spawn_create(tcb_t *caller, vaddr_t req_va, uint64_t req_len, tid_t *out_tid);
int spawn_wait_begin(tcb_t *caller, tid_t filter, tid_t *out_tid, int *out_status);
int spawn_fork_create(tcb_t *caller, tid_t *out_tid);
int spawn_exec(tcb_t *caller, vaddr_t req_va, uint64_t req_len);
#endif
