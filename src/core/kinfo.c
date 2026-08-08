#include "robu/kinfo.h"
#include "robu/vm.h"
#include "robu/pmm.h"
#include "robu/sched.h"
extern paddr_t boot_pml4;
static kinfo_page_t *kinfo;
void kinfo_init(uint32_t boot_apic_id, uint32_t cpu_count) {
    paddr_t frame = pmm_alloc(PMM_COLOR_ANY);
    kinfo = (kinfo_page_t *)frame;
    kinfo->abi_version_major = ROBU_ABI_VERSION_MAJOR;
    kinfo->abi_version_minor = ROBU_ABI_VERSION_MINOR;
    kinfo->feature_bits = KINFO_FEATURE_SMP | KINFO_FEATURE_ELF_LOADER;
    kinfo->cpu_count = cpu_count;
    kinfo->boot_apic_id = boot_apic_id;
    kinfo->clock_seq = 0;
    kinfo->clock_ticks = 0;
    kinfo->clock_hz = SCHED_HZ;
    arch_vm_map_page((paddr_t)&boot_pml4, KINFO_VA, frame, VM_PROT_READ | VM_PROT_USER);
}
void kinfo_set_devfs_tid(uint32_t tid) {
    kinfo->devfs_tid = tid;
}
void kinfo_set_test_report_tid(uint32_t tid) {
    kinfo->test_report_tid = tid;
}
void kinfo_set_benchserver_tid(uint32_t tid) {
    kinfo->benchserver_tid = tid;
}
void kinfo_set_abitest_helper_tid(uint32_t tid) {
    kinfo->abitest_helper_tid = tid;
}
void kinfo_set_abitest_slots(uint32_t untyped, uint32_t notif, uint32_t timer,
                             uint32_t helper_tcb, uint32_t revoke_frame) {
    kinfo->abitest_slots[0] = untyped;
    kinfo->abitest_slots[1] = notif;
    kinfo->abitest_slots[2] = timer;
    kinfo->abitest_slots[3] = helper_tcb;
    kinfo->abitest_slots[4] = revoke_frame;
}
void kinfo_set_ramfs_tid(uint32_t tid) {
    kinfo->ramfs_tid = tid;
}
void kinfo_set_abitest_exit_helper_tid(uint32_t tid) {
    kinfo->abitest_exit_helper_tid = tid;
}
void kinfo_set_procfs_tid(uint32_t tid) {
    kinfo->procfs_tid = tid;
}
void kinfo_set_sysfs_tid(uint32_t tid) {
    kinfo->sysfs_tid = tid;
}
int kinfo_mount_add(const char *prefix, uint32_t owner_tid) {
    int slot = -1;
    for (int i = 0; i < MOUNT_TABLE_MAX; i++) {
        if (!kinfo->mounts[i].in_use) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return -1;
    }
    kinfo->mount_seq++;
    asm volatile("" ::: "memory");
    kinfo->mounts[slot].owner_tid = owner_tid;
    int i = 0;
    for (; i < MOUNT_PREFIX_MAX - 1 && prefix[i]; i++) {
        kinfo->mounts[slot].prefix[i] = prefix[i];
    }
    kinfo->mounts[slot].prefix[i] = '\0';
    kinfo->mounts[slot].in_use = 1;
    asm volatile("" ::: "memory");
    kinfo->mount_seq++;
    return 0;
}
void kinfo_tick(uint64_t n) {
    kinfo->clock_seq++;
    asm volatile("" ::: "memory");
    kinfo->clock_ticks += n;
    asm volatile("" ::: "memory");
    kinfo->clock_seq++;
}
