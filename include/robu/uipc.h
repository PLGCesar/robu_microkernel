#ifndef ROBU_UIPC_H
#define ROBU_UIPC_H
#include "robu/types.h"
#include "robu/tcb.h"
#include "robu/ipc.h"
static inline int64_t robu_ipc_raw(uint64_t dest, uint64_t src_or_arg,
                                   uint64_t flags, msg_regs_t *io,
                                   tid_t *from_out) {
    register uint64_t r8 asm("r8") = io ? io->word[0] : 0;
    register uint64_t r9 asm("r9") = io ? io->word[1] : 0;
    register uint64_t r10 asm("r10") = io ? io->word[2] : 0;
    register uint64_t r12 asm("r12") = io ? io->word[3] : 0;
    register uint64_t r13 asm("r13") = io ? io->word[4] : 0;
    register uint64_t r14 asm("r14") = io ? io->word[5] : 0;
    register uint64_t rdi asm("rdi") = dest;
    register uint64_t rsi asm("rsi") = src_or_arg;
    register uint64_t rdx asm("rdx") = flags;
    int64_t status;
    asm volatile("int $0x30"
                 : "=a"(status), "+r"(rsi),
                   "+r"(r8), "+r"(r9), "+r"(r10),
                   "+r"(r12), "+r"(r13), "+r"(r14)
                 : "r"(rdi), "r"(rdx)
                 : "r11", "rcx", "memory", "cc");
    if (io) {
        io->word[0] = r8;
        io->word[1] = r9;
        io->word[2] = r10;
        io->word[3] = r12;
        io->word[4] = r13;
        io->word[5] = r14;
    }
    if (from_out) {
        *from_out = (tid_t)rsi;
    }
    return status;
}
static inline int64_t ipc_send(tid_t dest, msg_regs_t *msg) {
    return robu_ipc_raw(dest, 0, IPC_FLAG_NONE, msg, NULL);
}
static inline int64_t ipc_call(tid_t dest, msg_regs_t *io, tid_t *from) {
    return robu_ipc_raw(dest, dest, IPC_FLAG_RECV, io, from);
}
static inline int64_t ipc_reply_recv(tid_t dest, msg_regs_t *io, tid_t *from) {
    return robu_ipc_raw(dest, IPC_TID_ANY, IPC_FLAG_RECV, io, from);
}
static inline int64_t ipc_recv(tid_t filter, msg_regs_t *out, tid_t *from) {
    return robu_ipc_raw(0, filter, IPC_FLAG_RECV, out, from);
}
static inline void ipc_yield(void) {
    robu_ipc_raw(0, 0, IPC_FLAG_NONE, NULL, NULL);
}
static inline void ipc_sleep(uint64_t ticks) {
    robu_ipc_raw(0, ticks, IPC_FLAG_NONE, NULL, NULL);
}
static inline void ipc_exit(int status) __attribute__((noreturn));
static inline void ipc_exit(int status) {
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = (uint64_t)(uint8_t)status;
    robu_ipc_raw(0, 0, IPC_FLAG_EXIT, &m, NULL);
    for (;;) { }
}
#endif
