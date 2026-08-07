#ifndef ROBU_LIBC_PTY_H
#define ROBU_LIBC_PTY_H
int openpty(int *amaster, int *aslave, char *name, void *termp, void *winp);
int forkpty(int *amaster, char *name, void *termp, void *winp);
void login_tty(int fd);
#endif
