/*
 * Trap entry/exit — the floor every kernel activity stands on.
 *
 * Entry: the CPU pushes SS,RSP,RFLAGS,CS,RIP (always, in long mode) and
 * for some exceptions an error code, all onto the *interrupted thread's*
 * stack. The stub normalizes that (dummy error code + vector number),
 * pushes the 15 GPRs, hands the completed frame to C, and hops onto the
 * single per-CPU kernel stack. Because every gate is an interrupt gate
 * (IF cleared) and the kernel never re-enables interrupts, kernel
 * execution never nests — resetting RSP to kstack_top on every entry is
 * always correct, and no lock ever needs to be taken on this CPU.
 *
 * Exit: arch_enter_thread() points RSP at a saved frame (which lives in
 * some TCB), pops the GPRs and IRETQs. Starting a thread for the first
 * time and resuming one after a trap are the same operation.
 *
 * One subtlety entry has to handle: for a same-privilege trap (a
 * kernel-mode thread), the pushes above land on *that thread's own*
 * stack -- kstack_top is untouched until the explicit reset below. But
 * for a ring3->ring0 trap there is no such thing as "that thread's own
 * kernel-visible stack": the hardware itself switches to kstack_top
 * (via TSS.RSP0) before the very first push, so the whole frame is
 * built there. If we then reset RSP to kstack_top's absolute top and
 * `call trap_dispatch`, the call's own return-address push -- and
 * anything C code allocates after -- immediately starts overwriting
 * the tail of the frame we're about to hand it. So the frame is always
 * copied out to a fixed scratch buffer first, before RSP moves at all;
 * trap_dispatch never sees a frame that shares memory with its own
 * call stack, regardless of which kind of trap this was.
 *
 * Stage 2C: "the" kernel stack and "the" scratch buffer are now per
 * core (percpu.h), reached via %gs -- GS.BASE is set once per core at
 * its own startup and is independent of the GS *selector*, so no
 * swapgs is needed anywhere. kstack_top is a plain 8-byte value field,
 * read straight into RSP with one instruction (%gs:PERCPU_OFF_KSTACK_TOP).
 * trap_scratch is an embedded 176-byte buffer, not a scalar, so what's
 * needed is its *address*: read the self-pointer at %gs:0 (this core's
 * own percpu_t*) and add its offset.
 */

#include "percpu.h"

/* Exception vectors where the CPU pushes an error code. `==` evaluates
   to -1 (true) in the assembler, so the sum is nonzero iff \n matches. */
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

    /* Frame is complete (22 qwords). Copy it to THIS CORE's own scratch
       buffer *before* RSP moves anywhere -- see the file header for
       why this can't wait until after the reset. rep movsq needs
       DF=0 (forward copy), which holds throughout the kernel: nothing
       ever executes `std`. */
    mov %rsp, %rsi
    mov %gs:0, %rdi
    add $PERCPU_OFF_TRAP_SCRATCH, %rdi
    mov $22, %rcx
    rep movsq

    /* Recompute -- rep movsq advanced %rdi by 176 bytes copying it.
       This becomes trap_dispatch's one argument. */
    mov %gs:0, %rdi
    add $PERCPU_OFF_TRAP_SCRATCH, %rdi

    /* Now it's safe to move onto THIS CORE's own kernel stack and call
       into C. kstack_top is a plain value field -- no self-pointer
       indirection needed, straight into RSP. Every core's stack is
       16-aligned at its top (boot.S/ap_trampoline.S), so the ABI
       alignment rule (RSP % 16 == 0 at the call site) holds for free. */
    mov %gs:PERCPU_OFF_KSTACK_TOP, %rsp
    call trap_dispatch
    ud2                     /* trap_dispatch never returns */

.global arch_enter_thread
.type arch_enter_thread, @function
arch_enter_thread:
    mov %rdi, %rsp          /* RSP -> saved frame (inside a TCB) */
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
    add $16, %rsp           /* skip vector + error */
    iretq

/* Stub address table consumed by intr.c when building the IDT. */
.section .rodata
.global trap_stub_table
.align 8
trap_stub_table:
.irp n, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51
    .quad vector_stub_\n
.endr

.section .note.GNU-stack, "", @progbits
