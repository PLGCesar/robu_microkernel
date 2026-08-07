#ifndef ROBU_BOOT_H
#define ROBU_BOOT_H

#include "robu/types.h"
#include "robu/tcb.h"
#include "robu/kprintf.h"

#define PAGER_TID   1
#define MONITOR_TID 2

#define USER_CODE_VA  0x0000000040000000ULL
#define USER_STACK_VA 0x0000000040100000ULL

static inline uint64_t irq_save(void) {
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint64_t flags) {
    asm volatile("push %0; popfq" : : "r"(flags) : "memory", "cc");
}

static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

#define SAFE_PRINT(...) do {              \
        if (!quiet_mode) {                \
            uint64_t _f = irq_save();     \
            kprintf(__VA_ARGS__);         \
            irq_restore(_f);              \
        }                                 \
    } while (0)

extern const uint8_t *service_elf_start, *service_elf_end;
extern const uint8_t *task_elf_start, *task_elf_end;

tid_t spawn_ring3_task(const char *name, uint8_t prio);

void rootfs_init(void);
void root_task_init(paddr_t untyped_base, uint64_t untyped_size);
void ramfs_bin_seed_init(void);

tid_t devfs_init(void);
tid_t ramfs_init(void);
tid_t procfs_init(void);
tid_t sysfs_init(void);
void bench_init(void);

void abitest_init(paddr_t untyped_base, uint64_t untyped_size);
void argvtest_init(void);
void panic_test_init(void);
void test_exit_init(void);
void libctest_init(void);
void ramfstest_init(void);
void spawntest_init(void);
void consoletest_init(void);
void toybox_sh_c_init(void);
tid_t test_report_init(void);

tcb_t *toybox_spawn(const char *name, int argc, const char *const *argv, uint8_t prio);
void toybox_cmd_init(const char *name, uint8_t prio);
void toybox_cmd_init_arg(const char *name, uint8_t prio, const char *arg1);
void toybox_pipeline_advance(void);
void toybox_pipeline_init(void);

void ipc_echo_demo_init(void);
void ring3_task_demo_init(void);
void mmio_demo_init(void);
void xfer_demo_init(void);
void crash_kernel_demo_init(void);
void shm_demo_init(void);
void batch_demo_init(void);
void async_demo_init(void);
void sfi_demo_init(void);
void devfs_demo_init(void);
void timer_demo_init(paddr_t untyped_base);
void revoke_demo_init(paddr_t untyped_base);
void notif_latency_demo_init(paddr_t untyped_base);
void app_resilience_demo_init(void);

#endif
