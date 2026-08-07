#include "robu/types.h"
#include "robu/uipc.h"
void _start(void) {
    for (;;) {
        ipc_sleep(1000000);
    }
}
