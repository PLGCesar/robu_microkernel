#include <sys/reboot.h>
#include "robu/uipc.h"
#include "robu/ipc.h"

int reboot(int cmd) {
    (void)cmd;
    msg_regs_t m = (msg_regs_t){0};
    robu_ipc_raw(0, 0, IPC_FLAG_REBOOT, &m, NULL);
    return 0;
}
