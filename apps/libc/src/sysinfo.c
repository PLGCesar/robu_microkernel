#include <sys/sysinfo.h>
#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"

int sysinfo(struct sysinfo *info) {
    if (!info) {
        return -1;
    }
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = 0;
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_SYS_INFO, &m, NULL);
    if (rc != IPC_ERR_NONE) {
        return -1;
    }
    uint64_t total_frames = m.word[0];
    uint64_t free_frames = m.word[1];
    info->uptime = (long)(kinfo_read_ticks(kinfo_user()) / (kinfo_user()->clock_hz ? kinfo_user()->clock_hz : 1));
    info->loads[0] = info->loads[1] = info->loads[2] = 0;
    info->totalram = total_frames * 4096ull;
    info->freeram = free_frames * 4096ull;
    info->sharedram = 0;
    info->bufferram = 0;
    info->totalswap = 0;
    info->freeswap = 0;
    info->procs = 0;
    info->totalhigh = 0;
    info->freehigh = 0;
    info->mem_unit = 1;
    return 0;
}
