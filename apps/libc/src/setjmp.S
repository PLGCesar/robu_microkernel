/* Toybox-port foundation, milestone 4a: real setjmp/longjmp.
 *
 * lib/xwrap.c's _xexit() unconditionally references siglongjmp (the
 * `if (toys.rebound) siglongjmp(...)` branch exists in every one of
 * the 12 target commands' own exit path, not just sh's -- toys.rebound
 * is always NULL for a standalone command, so the branch is never
 * actually taken at runtime, but the call site still needs a real,
 * correct body to link at all). Standard x86_64 SysV callee-saved
 * register set (rbx, rbp, r12-r15) plus rsp and the return address --
 * the same minimal set every other libc's setjmp saves. No signal mask
 * to save/restore (this kernel has no real signal delivery yet), so
 * sigsetjmp/siglongjmp are thin wrappers that ignore the savesigs flag
 * rather than pretending to do something real.
 *
 * jmp_buf layout (apps/libc/include/setjmp.h): long[8] =
 *   [0]=rbx [1]=rbp [2]=r12 [3]=r13 [4]=r14 [5]=r15 [6]=rsp [7]=rip
 */
.text

.globl setjmp
.type setjmp, @function
setjmp:
    mov %rbx, 0(%rdi)
    mov %rbp, 8(%rdi)
    mov %r12, 16(%rdi)
    mov %r13, 24(%rdi)
    mov %r14, 32(%rdi)
    mov %r15, 40(%rdi)
    lea 8(%rsp), %rax      /* caller's rsp, i.e. after this call returns */
    mov %rax, 48(%rdi)
    mov (%rsp), %rax       /* return address */
    mov %rax, 56(%rdi)
    xor %eax, %eax
    ret

.globl longjmp
.type longjmp, @function
longjmp:
    mov %esi, %eax
    test %eax, %eax
    jnz 1f
    mov $1, %eax
1:
    mov 0(%rdi), %rbx
    mov 8(%rdi), %rbp
    mov 16(%rdi), %r12
    mov 24(%rdi), %r13
    mov 32(%rdi), %r14
    mov 40(%rdi), %r15
    mov 48(%rdi), %rsp
    mov 56(%rdi), %rdx
    jmp *%rdx

/* sigjmp_buf (long[9]) reuses jmp_buf's first 8 slots verbatim; slot
   [8] holds the savesigs flag purely for structural completeness --
   nothing reads it back, since there's no real mask to restore. */
.globl sigsetjmp
.type sigsetjmp, @function
sigsetjmp:
    mov %esi, 64(%rdi)
    jmp setjmp

.globl siglongjmp
.type siglongjmp, @function
siglongjmp:
    jmp longjmp
