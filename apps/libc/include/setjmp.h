#ifndef ROBU_LIBC_SETJMP_H
#define ROBU_LIBC_SETJMP_H
typedef long jmp_buf[8];
typedef long sigjmp_buf[9];
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val) __attribute__((noreturn));
int sigsetjmp(sigjmp_buf env, int savesigs);
void siglongjmp(sigjmp_buf env, int val) __attribute__((noreturn));
#endif
