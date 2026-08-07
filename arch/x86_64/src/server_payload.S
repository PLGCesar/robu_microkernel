.section .rodata
.global server_payload_start, server_payload_end
.align 16
.code64
server_payload_start:
server_loop:
    /* recv(ANY): dest=0, filter=0 (IPC_TID_ANY), flags=IPC_FLAG_RECV */
    xor %rdi, %rdi
    xor %rsi, %rsi
    mov $2, %rdx
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r12, %r12
    xor %r13, %r13
    xor %r14, %r14
    int $0x30
    /* rsi = sender tid, r8 = request word */

    mov %rsi, %rbx           /* keep the sender's tid past the reply setup */

    cmp $0xDEAD, %r8
    je server_crash

    /* reply(sender): dest=sender, flags=NONE, payload = request + 1 */
    inc %r8
    mov %rbx, %rdi
    xor %rsi, %rsi
    xor %rdx, %rdx
    int $0x30

    jmp server_loop

server_crash:
    ud2                      /* deliberate #UD -- proves crash isolation */
server_payload_end:

.section .note.GNU-stack, "", @progbits
