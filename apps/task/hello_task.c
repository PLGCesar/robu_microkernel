#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#define MONITOR_TID 2
void _start(void) {
    uint64_t iterations = 0;
    const volatile kinfo_page_t *kinfo = (const volatile kinfo_page_t *)KINFO_VA;
    for (;;) {
        iterations++;
        msg_regs_t m;
        m.word[0] = iterations;
        m.word[1] = (uint64_t)&_start;
        m.word[2] = ((uint64_t)kinfo->abi_version_major << 16) | kinfo->abi_version_minor;
        m.word[3] = kinfo_read_ticks(kinfo);
        ipc_send(MONITOR_TID, &m);
        volatile uint64_t delay = 3000000;
        while (delay--) {
        }
    }
}
