/*
 * A ring-3 "server" that can be told to crash, hand-written like
 * user_payload.S (same reasons: no ELF loader, so raw bytes get copied
 * into a mapped executable page -- see spawn_crash_server() in main.c).
 * Position-independent for the same reason: only local, PC-relative
 * jumps, no reference to its own link-time address.
 *
 * Loop forever: block for a request from any client (int $0x30,
 * IPC_FLAG_RECV, dest=0). If the request word is the poison value
 * (0xDEAD), execute ud2 -- a deliberate, deterministic #UD -- so a test
 * driver can trigger a real crash on demand instead of waiting for an
 * organic bug. Otherwise reply with the request incremented by one and
 * go back to waiting. This is the stage 2B reliability demo: the
 * supervisor in main.c notices this thread go THREAD_STATE_DEAD and
 * respawns a fresh one, and any client mid-call to the dead one gets
 * unblocked with IPC_ERR_NOT_FOUND (see sched_terminate_current())
 * rather than hanging forever.
 */

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
