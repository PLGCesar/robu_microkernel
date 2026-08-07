/*
 * Stage 0C: the code a retyped child thread actually runs. Hand-written
 * like server_payload.S/user_payload.S (same reasons: this is going to
 * be memcpy'd by root_task.c into a physical frame it just retyped from
 * untyped memory and mapped into a brand-new address space -- there is
 * no ELF loader involved for an object minted by retype). RIP-relative
 * addressing on purpose: root_task.c decides at runtime which VA this
 * ends up mapped at (ROOT_TASK_ENTRY_VA), and this code must work
 * unchanged regardless of that choice.
 *
 * Deliberately touches no stack at all (no push/pop/call/ret) -- the
 * retyped child gets no second frame for one, and doesn't need one:
 * increment a counter, yield, repeat.
 */

.section .rodata
.global child_payload_start, child_payload_end
.global child_payload_counter_offset
.align 16
.code64
child_payload_start:
loop:
    incl counter(%rip)  /* proof of life: root_task.c re-reads this
                            through its own mapping of the same
                            physical frame */
    xor  %edi, %edi     /* dest = 0 */
    xor  %esi, %esi     /* filter = 0 -> yield */
    xor  %edx, %edx     /* flags = 0 */
    int  $0x30
    jmp  loop

.align 4
counter:
    .long 0
child_payload_end:

/* Link-time-constant byte offset of `counter` within the payload, so
   root_task.c can find it without hand-computing instruction lengths. */
.align 8
child_payload_counter_offset:
    .quad counter - child_payload_start

.section .note.GNU-stack, "", @progbits
