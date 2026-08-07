#ifndef ARCH_X86_64_CONTEXT_H
#define ARCH_X86_64_CONTEXT_H
#include "robu/types.h"
typedef struct arch_uctx {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error;
    uint64_t rip, cs, rflags, rsp, ss;
} arch_uctx_t;
#define TRAP_VEC_PAGEFAULT  14
#define TRAP_VEC_TIMER      32
#define TRAP_VEC_IPC        0x30
#define TRAP_VEC_IPI_KICK   0x31
#define TRAP_VEC_IPI_PANIC  0x32
#define TRAP_VEC_IPI_SHOOTDOWN 0x33
void arch_enter_thread(arch_uctx_t *uctx) __attribute__((noreturn));
void arch_uctx_init(arch_uctx_t *uctx, void (*entry)(void), void *stack_top);
void arch_uctx_init_user(arch_uctx_t *uctx, vaddr_t entry, vaddr_t user_stack_top);
void arch_uctx_init_user_argv(arch_uctx_t *uctx, vaddr_t entry, vaddr_t user_stack_top,
                              uint64_t argc, uint64_t argv, uint64_t envp, uint64_t heap_base,
                              uint64_t spawn_info);
void trap_dispatch(arch_uctx_t *frame) __attribute__((noreturn));
#endif
