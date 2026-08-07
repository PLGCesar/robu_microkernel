#define MONITOR_TID   2
#define USER_HEAP_VA  0x40200000

.section .rodata
.global user_payload_start, user_payload_end
.align 16
.code64
user_payload_start:
    xor %rbx, %rbx

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
