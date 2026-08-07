#include "robu/types.h"
#include "robu/uipc.h"
void _start(void) {
    msg_regs_t m;
    tid_t from;
    ipc_recv(IPC_TID_ANY, &m, &from);
    for (;;) {
        ipc_reply_recv(from, &m, &from);
    }
}
