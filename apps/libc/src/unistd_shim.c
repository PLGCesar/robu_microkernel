#include <unistd.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <string.h>
#include <errno.h>
#include "robu/types.h"
#include "robu/uipc.h"
int rmdir(const char *path) { (void)path; errno = ENOSYS; return -1; }
int execve(const char *path, char *const argv[], char *const envp[]) {
    (void)path; (void)argv; (void)envp;
    errno = ENOSYS;
    return -1;
}
int execvp(const char *file, char *const argv[]) { (void)file; (void)argv; errno = ENOSYS; return -1; }
int execv(const char *path, char *const argv[]) { (void)path; (void)argv; errno = ENOSYS; return -1; }
pid_t fork(void) {
    msg_regs_t m = (msg_regs_t){0};
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_FORK, &m, NULL);
    if (rc != IPC_ERR_NONE) {
        errno = (rc == IPC_ERR_NO_MEM) ? ENOMEM : EAGAIN;
        return -1;
    }
    return (pid_t)m.word[0];
}
pid_t vfork(void) {
    return fork();
}
pid_t setsid(void) { errno = ENOSYS; return -1; }
int chroot(const char *path) { (void)path; errno = ENOSYS; return -1; }
int initgroups(const char *user, gid_t group) { (void)user; (void)group; errno = ENOSYS; return -1; }
pid_t getpid(void) { return 1; }
pid_t getppid(void) { return 0; }
pid_t getsid(pid_t pid) { (void)pid; return 1; }
pid_t getpgid(pid_t pid) { (void)pid; return 1; }
pid_t getpgrp(void) { return 1; }
int setpgid(pid_t pid, pid_t pgid) { (void)pid; (void)pgid; return 0; }
pid_t tcgetpgrp(int fd) { (void)fd; return 1; }
int tcsetpgrp(int fd, pid_t pgrp) { (void)fd; (void)pgrp; return 0; }
uid_t getuid(void) { return 0; }
gid_t getgid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getegid(void) { return 0; }
int setuid(uid_t uid) { (void)uid; return 0; }
int setgid(gid_t gid) { (void)gid; return 0; }
int seteuid(uid_t uid) { (void)uid; return 0; }
int setegid(gid_t gid) { (void)gid; return 0; }
int link(const char *oldpath, const char *newpath) { (void)oldpath; (void)newpath; errno = ENOSYS; return -1; }
int linkat(int a, const char *b, int c, const char *d, int e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    errno = ENOSYS;
    return -1;
}
int symlink(const char *target, const char *linkpath) { (void)target; (void)linkpath; errno = ENOSYS; return -1; }
int symlinkat(const char *target, int newdirfd, const char *linkpath) {
    (void)target; (void)newdirfd; (void)linkpath;
    errno = ENOSYS;
    return -1;
}
ssize_t readlink(const char *path, char *buf, size_t bufsiz) { (void)path; (void)buf; (void)bufsiz; errno = ENOSYS; return -1; }
ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz) {
    (void)dirfd; (void)path; (void)buf; (void)bufsiz;
    errno = ENOSYS;
    return -1;
}
unsigned int sleep(unsigned int seconds) {
    ipc_sleep((uint64_t)seconds * 1000000);
    return 0;
}
int ftruncate(int fd, off_t length) { (void)fd; (void)length; errno = ENOSYS; return -1; }
int truncate(const char *path, off_t length) { (void)path; (void)length; errno = ENOSYS; return -1; }
int chown(const char *path, uid_t owner, gid_t group) { (void)path; (void)owner; (void)group; errno = ENOSYS; return -1; }
int fchown(int fd, uid_t owner, gid_t group) { (void)fd; (void)owner; (void)group; errno = ENOSYS; return -1; }
int lchown(const char *path, uid_t owner, gid_t group) { (void)path; (void)owner; (void)group; errno = ENOSYS; return -1; }
int fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flags) {
    (void)dirfd; (void)path; (void)owner; (void)group; (void)flags;
    errno = ENOSYS;
    return -1;
}
int uname(struct utsname *buf) {
    if (!buf) {
        return -1;
    }
    memset(buf, 0, sizeof(*buf));
    strlcpy(buf->sysname, "Robu", sizeof(buf->sysname));
    strlcpy(buf->nodename, "robu", sizeof(buf->nodename));
    strlcpy(buf->release, "0.9", sizeof(buf->release));
    strlcpy(buf->version, "sh-replacement", sizeof(buf->version));
    strlcpy(buf->machine, "x86_64", sizeof(buf->machine));
    return 0;
}
int gethostname(char *name, size_t len) {
    static const char host[] = "robu";
    if (len < sizeof(host)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    for (size_t i = 0; i < sizeof(host); i++) {
        name[i] = host[i];
    }
    return 0;
}
int pause(void) {
    for (;;) {
        ipc_sleep(1000000);
    }
}
unsigned int alarm(unsigned int seconds) { (void)seconds; return 0; }
long sysconf(int name) {
    (void)name;
    return _SC_ARG_MAX;
}
