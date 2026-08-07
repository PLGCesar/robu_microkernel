#ifndef ROBU_LIBC_UNISTD_H
#define ROBU_LIBC_UNISTD_H
#include <sys/types.h>
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
extern char **environ;
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char *path);
int unlinkat(int dirfd, const char *path, int flags);
int rmdir(const char *path);
int chdir(const char *path);
int fchdir(int fd);
char *getcwd(char *buf, size_t size);
int access(const char *path, int mode);
int isatty(int fd);
int dup(int fd);
int dup2(int oldfd, int newfd);
int pipe(int fds[2]);
pid_t fork(void);
pid_t vfork(void);
int execve(const char *path, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);
int execv(const char *path, char *const argv[]);
pid_t setsid(void);
int chroot(const char *path);
int initgroups(const char *user, gid_t group);
pid_t getpid(void);
pid_t getppid(void);
pid_t getsid(pid_t pid);
pid_t getpgid(pid_t pid);
pid_t getpgrp(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int faccessat(int dirfd, const char *path, int mode, int flags);
uid_t getuid(void);
gid_t getgid(void);
uid_t geteuid(void);
gid_t getegid(void);
void _exit(int status) __attribute__((noreturn));
int link(const char *oldpath, const char *newpath);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
int symlink(const char *target, const char *linkpath);
int symlinkat(const char *target, int newdirfd, const char *linkpath);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz);
unsigned int sleep(unsigned int seconds);
int fsync(int fd);
int ftruncate(int fd, off_t length);
int truncate(const char *path, off_t length);
int chown(const char *path, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int lchown(const char *path, uid_t owner, gid_t group);
int fchownat(int dirfd, const char *path, uid_t owner, gid_t group, int flags);
int setuid(uid_t uid);
int setgid(gid_t gid);
int seteuid(uid_t uid);
int setegid(gid_t gid);
int gethostname(char *name, size_t len);
int pause(void);
unsigned int alarm(unsigned int seconds);
long sysconf(int name);
#define _SC_ARG_MAX 131072
#endif
