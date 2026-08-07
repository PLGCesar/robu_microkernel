/*
 * A ring-3 task, hand-written.
 *
 * There is no ELF loader or filesystem yet, so this is the standard
 * bring-up trick: write the raw instructions directly, mark the range
 * with symbols, and have the kernel memcpy those bytes into a page it
 * maps executable-for-user at a fixed virtual address (see
 * spawn_ring3_task() in main.c). It has to be genuinely
 * position-independent for that to work -- no reference to its own
 * link-time address anywhere -- so every branch here is a local,
 * PC-relative short jump, and the only "addresses" it touches are
 * immediates the kernel and this code both agree on out of band
 * (USER_HEAP_VA). It never returns and never touches the stack beyond
 * what IRETQ needs to get it here in the first place.
 *
 * What it does, forever: ping a fixed monitor tid with a counter
 * (int $0x30, IPC_FLAG_NONE -- fire and forget, no reply expected),
 * write through a heap address that starts out deliberately unmapped
 * (the first write takes a real #PF that a pager resolves lazily),
 * then spin briefly so it doesn't flood the console.
 *
 * Register ABI for the trap (see robu/ipc.h): rdi=dest tid, rsi=recv
 * filter/flags-dependent arg, rdx=IPC flags, r8/r9/r10/r12/r13/r14=six
 * payload words. IPC_FLAG_NONE has no receive phase, so rsi is unused
 * on entry here (zeroed for cleanliness) and every payload word past
 * the counter is zeroed too.
 */

#define MONITOR_TID   2
/* Must be >= 1 GiB: below that is the kernel's shared identity map
 * (see vm_arch.c's arch_vm_create_address_space), where every address
 * space sees the exact same physical pages -- there is no "unmapped"
 * to lazily fault on down there. Must match main.c's USER_HEAP_VA. */
#define USER_HEAP_VA  0x40200000

.section .rodata
.global user_payload_start, user_payload_end
.align 16
.code64
user_payload_start:
    xor %rbx, %rbx              /* rbx = iteration counter */

payload_loop:
    inc %rbx

    mov $MONITOR_TID, %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    mov %rbx, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    int $0x30

    mov $USER_HEAP_VA, %rax
    mov %rbx, (%rax)

    mov $3000000, %rcx
payload_delay:
    dec %rcx
    jnz payload_delay

    jmp payload_loop
user_payload_end:

.section .note.GNU-stack, "", @progbits
