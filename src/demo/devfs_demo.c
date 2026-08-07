#include "robu/types.h"
#include "robu/arch.h"
#include "robu/sched.h"
#include "robu/uipc.h"
#include "robu/devfs.h"
#include "../boot.h"

static uint8_t stack_devfs_demo[STACK_SIZE] __attribute__((aligned(16)));

static void devfs_demo_entry(void) {
    uint8_t buf[16], buf2[16];
    int64_t h, s;
    h = devfs_open("/dev/bogus");
    SAFE_PRINT("[devfs-demo] open(/dev/bogus) -> %ld (expect < 0)\n", h);
    h = devfs_open("/dev/null");
    SAFE_PRINT("[devfs-demo] open(/dev/null) -> handle=%ld\n", h);
    s = devfs_write((uint64_t)h, "0123456789", 10);
    SAFE_PRINT("[devfs-demo] write(/dev/null, 10 bytes) -> %ld (expect 10, discarded)\n", s);
    s = devfs_read((uint64_t)h, buf, sizeof(buf));
    SAFE_PRINT("[devfs-demo] read(/dev/null) -> %ld (expect 0)\n", s);
    devfs_close((uint64_t)h);
    h = devfs_open("/dev/zero");
    s = devfs_read((uint64_t)h, buf, sizeof(buf));
    int all_zero = 1;
    for (int64_t i = 0; i < s; i++) {
        if (buf[i] != 0) {
            all_zero = 0;
        }
    }
    SAFE_PRINT("[devfs-demo] read(/dev/zero, 16) -> %ld bytes, all_zero=%d (expect 1)\n", s, all_zero);
    devfs_close((uint64_t)h);
    h = devfs_open("/dev/random");
    s = devfs_read((uint64_t)h, buf, sizeof(buf));
    int64_t s2 = devfs_read((uint64_t)h, buf2, sizeof(buf2));
    int differ = 0;
    for (int64_t i = 0; i < s && i < s2 && i < (int64_t)sizeof(buf); i++) {
        if (buf[i] != buf2[i]) {
            differ = 1;
        }
    }
    SAFE_PRINT("[devfs-demo] random: read1=%ld read2=%ld differ=%d (expect 1 if RDRAND available)\n",
            s, s2, differ);
    devfs_close((uint64_t)h);
    h = devfs_open("/dev/console");
    if (!quiet_mode) {
        const char *msg = "[devfs-demo] hello through /dev/console, relayed by the kernel's console-write verb\n";
        s = devfs_write((uint64_t)h, msg, strlen(msg));
        SAFE_PRINT("[devfs-demo] write(/dev/console) -> %ld\n", s);
    }
    devfs_close((uint64_t)h);
    s = devfs_read(99, buf, sizeof(buf));
    SAFE_PRINT("[devfs-demo] read(stale handle=99) -> %ld (expect < 0)\n", s);
    s = devfs_write(99, buf, sizeof(buf));
    SAFE_PRINT("[devfs-demo] write(stale handle=99) -> %ld (expect < 0)\n", s);
    int rc = devfs_kernel_console_write((const uint8_t *)"unauthorized", 12);
    SAFE_PRINT("[devfs-demo] direct IPC_FLAG_CONSOLE_WRITE (not devfs's tid) -> %d (expect %d, denied)\n",
            rc, IPC_ERR_NO_CAP);
    SAFE_PRINT("[devfs-demo] all scenarios complete\n");
    for (;;) {
        ipc_sleep(SCHED_HZ * 10);
    }
}

void devfs_demo_init(void) {
    thread_create("devfs-demo", devfs_demo_entry, stack_devfs_demo + STACK_SIZE, 9);
}
