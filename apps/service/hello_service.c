#include "robu/types.h"
#include "robu/uipc.h"
#define POISON 0xDEAD
void _start(void) {
    msg_regs_t m;
    tid_t from;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        if (m.word[0] == POISON) {
            asm volatile("ud2");
        }
        m.word[0]++;
        ipc_send(from, &m);
    }
}
