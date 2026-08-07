#ifndef ROBU_LIBC_SYS_WAIT_H
#define ROBU_LIBC_SYS_WAIT_H
#include <sys/types.h>
#define WNOHANG   1
#define WUNTRACED 2
#define WIFEXITED(status)   (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) >> 8) & 0xff)
#define WIFSIGNALED(status) ((((status) & 0x7f) + 1) >> 1 > 0)
#define WTERMSIG(status)    ((status) & 0x7f)
#define WIFSTOPPED(status)  (((status) & 0xff) == 0x7f)
#define WSTOPSIG(status)    (((status) >> 8) & 0xff)
#define WIFCONTINUED(status) ((status) == 0xffff)
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
struct rusage;
pid_t wait4(pid_t pid, int *status, int options, struct rusage *usage);
#endif
