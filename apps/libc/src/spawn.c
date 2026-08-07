#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/spawn.h"
#include "libc_internal.h"
int __libc_spawn(const char *name, char *const argv[], char *const envp[]) {
    static uint8_t buf[SPAWN_REQ_MAX_LEN];
    robu_spawn_req_t *req = (robu_spawn_req_t *)buf;
    int argc = 0;
    while (argv && argv[argc]) {
        argc++;
    }
    int envc = 0;
    while (envp && envp[envc]) {
        envc++;
    }
    if (argc > SPAWN_MAX_ARGS || envc > SPAWN_MAX_ARGS) {
        errno = E2BIG;
        return -1;
    }
    robu_spawn_str_t argv_table[SPAWN_MAX_ARGS];
    robu_spawn_str_t envp_table[SPAWN_MAX_ARGS];
    uint32_t off = sizeof(*req);
    size_t name_len = strlen(name);
    if ((uint64_t)off + name_len > sizeof(buf)) {
        errno = E2BIG;
        return -1;
    }
    uint32_t name_off = off;
    memcpy(buf + off, name, name_len);
    off += (uint32_t)name_len;
    for (int i = 0; i < argc; i++) {
        size_t len = strlen(argv[i]);
        if ((uint64_t)off + len > sizeof(buf)) {
            errno = E2BIG;
            return -1;
        }
        argv_table[i].off = off;
        argv_table[i].len = (uint32_t)len;
        memcpy(buf + off, argv[i], len);
        off += (uint32_t)len;
    }
    for (int i = 0; i < envc; i++) {
        size_t len = strlen(envp[i]);
        if ((uint64_t)off + len > sizeof(buf)) {
            errno = E2BIG;
            return -1;
        }
        envp_table[i].off = off;
        envp_table[i].len = (uint32_t)len;
        memcpy(buf + off, envp[i], len);
        off += (uint32_t)len;
    }
    uint32_t argv_off = off;
    if ((uint64_t)off + (uint64_t)argc * sizeof(robu_spawn_str_t) > sizeof(buf)) {
        errno = E2BIG;
        return -1;
    }
    memcpy(buf + off, argv_table, (size_t)argc * sizeof(robu_spawn_str_t));
    off += (uint32_t)argc * (uint32_t)sizeof(robu_spawn_str_t);
    uint32_t envp_off = off;
    if ((uint64_t)off + (uint64_t)envc * sizeof(robu_spawn_str_t) > sizeof(buf)) {
        errno = E2BIG;
        return -1;
    }
    memcpy(buf + off, envp_table, (size_t)envc * sizeof(robu_spawn_str_t));
    off += (uint32_t)envc * (uint32_t)sizeof(robu_spawn_str_t);
    robu_spawn_fd_t fd_table_out[SPAWN_FD_INFO_MAX];
    uint32_t nfds = 0;
    for (int fd = 0; fd <= 2; fd++) {
        uint32_t kind;
        uint64_t handle;
        if (__libc_fd_export(fd, &kind, &handle) == 0) {
            fd_table_out[nfds].fd = (uint32_t)fd;
            fd_table_out[nfds].kind = kind;
            fd_table_out[nfds].handle = handle;
            nfds++;
        }
    }
    uint32_t fds_off = off;
    if (nfds > 0) {
        if ((uint64_t)off + (uint64_t)nfds * sizeof(robu_spawn_fd_t) > sizeof(buf)) {
            errno = E2BIG;
            return -1;
        }
        memcpy(buf + off, fd_table_out, (size_t)nfds * sizeof(robu_spawn_fd_t));
        off += (uint32_t)nfds * (uint32_t)sizeof(robu_spawn_fd_t);
    }
    req->magic = SPAWN_REQ_MAGIC;
    req->total_len = off;
    req->name_off = name_off;
    req->name_len = (uint32_t)name_len;
    req->argc = (uint32_t)argc;
    req->argv_off = argc ? argv_off : 0;
    req->envc = (uint32_t)envc;
    req->envp_off = envc ? envp_off : 0;
    req->nfds = nfds;
    req->fds_off = nfds ? fds_off : 0;
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = (uint64_t)buf;
    m.word[1] = off;
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_SPAWN, &m, NULL);
    if (rc != IPC_ERR_NONE) {
        errno = (rc == IPC_ERR_NOT_FOUND) ? ENOENT
              : (rc == IPC_ERR_NO_MEM) ? ENOMEM : EINVAL;
        return -1;
    }
    return (int)m.word[0];
}
pid_t waitpid(pid_t pid, int *status, int options) {
    tid_t filter = (pid > 0) ? (tid_t)pid : 0;
    uint32_t flags = IPC_FLAG_WAIT;
    if (options & WNOHANG) {
        flags |= IPC_FLAG_NOBLOCK;
    }
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = filter;
    int64_t rc = robu_ipc_raw(0, 0, flags, &m, NULL);
    if (rc == IPC_ERR_WOULDBLOCK) {
        return 0;
    }
    if (rc != IPC_ERR_NONE) {
        errno = (rc == IPC_ERR_NOT_FOUND) ? ECHILD : EINVAL;
        return -1;
    }
    if (status) {
        *status = ((int)m.word[1] & 0xff) << 8;
    }
    return (pid_t)m.word[0];
}
pid_t wait(int *status) {
    return waitpid(-1, status, 0);
}
pid_t wait4(pid_t pid, int *status, int options, struct rusage *usage) {
    (void)pid;
    (void)status;
    (void)options;
    (void)usage;
    errno = ENOSYS;
    return -1;
}
int pipe(int fds[2]) {
    msg_regs_t m = (msg_regs_t){0};
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_PIPE_CREATE, &m, NULL);
    if (rc != IPC_ERR_NONE) {
        errno = ENOMEM;
        return -1;
    }
    uint64_t handle = m.word[0];
    int rfd = __libc_fd_alloc_pipe(handle, 0);
    if (rfd < 0) {
        msg_regs_t cr = (msg_regs_t){0};
        cr.word[0] = handle;
        cr.word[1] = 0;
        robu_ipc_raw(0, 0, IPC_FLAG_PIPE_CLOSE, &cr, NULL);
        msg_regs_t cw = (msg_regs_t){0};
        cw.word[0] = handle;
        cw.word[1] = 1;
        robu_ipc_raw(0, 0, IPC_FLAG_PIPE_CLOSE, &cw, NULL);
        errno = EMFILE;
        return -1;
    }
    int wfd = __libc_fd_alloc_pipe(handle, 1);
    if (wfd < 0) {
        close(rfd);
        msg_regs_t cw = (msg_regs_t){0};
        cw.word[0] = handle;
        cw.word[1] = 1;
        robu_ipc_raw(0, 0, IPC_FLAG_PIPE_CLOSE, &cw, NULL);
        errno = EMFILE;
        return -1;
    }
    fds[0] = rfd;
    fds[1] = wfd;
    return 0;
}
