#ifndef ROBU_KINFO_H
#define ROBU_KINFO_H
#include "robu/types.h"
#define KINFO_VA 0x0000000080000000ULL
#define ROBU_ABI_VERSION_MAJOR 1
#define ROBU_ABI_VERSION_MINOR 0
#define KINFO_FEATURE_SMP        (1ULL << 0)
#define KINFO_FEATURE_ELF_LOADER (1ULL << 1)
typedef struct {
    uint32_t abi_version_major;
    uint32_t abi_version_minor;
    uint64_t feature_bits;
    uint32_t cpu_count;
    uint32_t boot_apic_id;
    volatile uint32_t clock_seq;
    uint64_t clock_ticks;
    uint32_t clock_hz;
    uint32_t devfs_tid;
    uint32_t test_report_tid;
    uint32_t benchserver_tid;
    uint32_t abitest_helper_tid;
    uint32_t abitest_slots[5];
    uint32_t ramfs_tid;
    uint32_t abitest_exit_helper_tid;
    uint32_t procfs_tid;
    uint32_t sysfs_tid;
} kinfo_page_t;
static inline uint64_t kinfo_read_ticks(const volatile kinfo_page_t *k) {
    uint32_t seq0, seq1;
    uint64_t ticks;
    do {
        seq0 = k->clock_seq;
        asm volatile("" ::: "memory");
        ticks = k->clock_ticks;
        asm volatile("" ::: "memory");
        seq1 = k->clock_seq;
    } while (seq0 != seq1 || (seq0 & 1u));
    return ticks;
}
void kinfo_init(uint32_t boot_apic_id, uint32_t cpu_count);
void kinfo_set_devfs_tid(uint32_t tid);
void kinfo_set_test_report_tid(uint32_t tid);
void kinfo_set_benchserver_tid(uint32_t tid);
void kinfo_set_abitest_helper_tid(uint32_t tid);
void kinfo_set_abitest_slots(uint32_t untyped, uint32_t notif, uint32_t timer,
                             uint32_t helper_tcb, uint32_t revoke_frame);
void kinfo_set_ramfs_tid(uint32_t tid);
void kinfo_set_abitest_exit_helper_tid(uint32_t tid);
void kinfo_set_procfs_tid(uint32_t tid);
void kinfo_set_sysfs_tid(uint32_t tid);
static inline const kinfo_page_t *kinfo_user(void) {
    return (const kinfo_page_t *)KINFO_VA;
}
void kinfo_tick(uint64_t n);
#endif
