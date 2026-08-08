#include "percpu.h"
#define HAS_ERRCODE(n) ((n == 8) + (n == 10) + (n == 11) + (n == 12) + \
                        (n == 13) + (n == 14) + (n == 17) + (n == 21) + (n == 30))

.section .text

.macro TRAP_STUB n
.align 8
vector_stub_\n:
    .if HAS_ERRCODE(\n)
    .else
    pushq $0
    .endif
    pushq $\n
    jmp trap_common
.endm

.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51
TRAP_STUB \n
.endr

trap_common:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, %rsi
    mov %gs:0, %rdi
    add $PERCPU_OFF_TRAP_SCRATCH, %rdi
    mov $22, %rcx
    rep movsq

    mov %gs:0, %rdi
    add $PERCPU_OFF_TRAP_SCRATCH, %rdi

    mov %gs:PERCPU_OFF_KSTACK_TOP, %rsp
    call trap_dispatch
    ud2

.global arch_enter_thread_raw
.type arch_enter_thread_raw, @function
arch_enter_thread_raw:
    mov %rdi, %rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    add $16, %rsp
    iretq

.section .rodata
.global trap_stub_table
.align 8
trap_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51
    .quad vector_stub_\n
.endr

.section .note.GNU-stack, "", @progbits
