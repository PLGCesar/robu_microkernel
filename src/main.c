#include "robu/types.h"
#include "robu/arch.h"
#include "robu/kprintf.h"
#include "robu/sched.h"
#include "robu/vm.h"
#include "robu/pmm.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#include "robu/cmdline.h"
#include "robu/untyped.h"
#include "lapic.h"
#include "smp.h"
#include "percpu.h"
#include "boot.h"

#define UNTYPED_REGION_SIZE (128u * 1024)

int quiet_mode;

static uint8_t stack_console_in[STACK_SIZE] __attribute__((aligned(16)));
static void console_in_entry(void) {
    for (;;) {
        int c;
        while ((c = arch_console_getc()) >= 0) {
            arch_console_line_feed(c);
        }
        ipc_sleep(1);
    }
}

static uint8_t stack_pager[STACK_SIZE] __attribute__((aligned(16)));
static volatile uint64_t pager_faults_resolved;
static void pager_entry(void) {
    msg_regs_t m;
    tid_t from;
    int color_cursor = 0;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        vaddr_t fault_addr = m.word[0];
        tid_t faulter = (tid_t)m.word[2];
        tcb_t *ft = sched_get_tcb(faulter);
        if (!ft) {
            continue;
        }
        vaddr_t page_va = fault_addr & ~(PAGE_SIZE_4K - 1);
        int color = color_cursor++ & (PMM_NUM_COLORS - 1);
        paddr_t frame = pmm_alloc(color);
        arch_vm_map_page(ft->address_space, page_va, frame,
                         VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER);
        pager_faults_resolved++;
        SAFE_PRINT("[pager] tid=%u '%s' faulted at 0x%lx rip=0x%lx -> frame 0x%lx "
                   "(color %d), mapped and resumed (#%lu)\n",
                   faulter, ft->name, fault_addr, ft->uctx.rip, frame, color, pager_faults_resolved);
        ipc_send(faulter, NULL);
    }
}

static uint8_t stack_monitor[STACK_SIZE] __attribute__((aligned(16)));
static void monitor_entry(void) {
    msg_regs_t m;
    tid_t from;
    uint64_t total = 0;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        total++;
        if (total % 25 == 0) {
            SAFE_PRINT("[monitor] ring-3 tid=%u ping #%lu addr=0x%lx "
                       "kinfo(ver=%lu.%lu tick=%lu) (%lu pings received total)\n",
                       from, m.word[0], m.word[1],
                       m.word[2] >> 16, m.word[2] & 0xFFFF, m.word[3], total);
        }
    }
}

void kmain(void) {
    arch_console_init();
    arch_fpu_boot_init();
    kputs("");
    kputs("Robu Kernel 0.9 x86_64 booting...");
    uint32_t boot_magic;
    if (arch_boot_magic(&boot_magic) == 0) {
        kprintf("[boot] robu_kernel -- loader magic 0x%x (valid), cores=%u\n",
                boot_magic, (unsigned)MAX_CPUS);
    } else {
        kprintf("[boot] robu_kernel -- loader magic unavailable, cores=%u\n",
                (unsigned)MAX_CPUS);
    }
    paddr_t mem_base;
    uint64_t mem_len;
    arch_detect_memory(&mem_base, &mem_len);
    rootfs_init();
    quiet_mode = cmdline_get("quiet") != NULL;
    uint64_t effective_len = mem_len;
    if (mem_base < ROBU_IDENTITY_MAP_LIMIT) {
        uint64_t max_len = ROBU_IDENTITY_MAP_LIMIT - mem_base;
        if (effective_len > max_len) {
            effective_len = max_len;
        }
    } else {
        effective_len = 0;
    }
    if (cmdline_get("force_fatal")) {
        effective_len = 0;
    }
    if (effective_len < UNTYPED_REGION_SIZE) {
        kprintf("[boot] FATAL: not enough identity-mapped RAM for the untyped region "
                "(have %lu bytes, need %lu)\n",
                effective_len, (uint64_t)UNTYPED_REGION_SIZE);
        for (;;) { asm volatile("cli; hlt"); }
    }
    uint64_t pmm_len = effective_len - UNTYPED_REGION_SIZE;
    paddr_t untyped_base = mem_base + pmm_len;
    pmm_init(mem_base, pmm_len);
    untyped_init(untyped_base, UNTYPED_REGION_SIZE);
    kprintf("[boot] untyped region: %lu bytes at 0x%lx\n", (uint64_t)UNTYPED_REGION_SIZE, untyped_base);
    kprintf("[boot] frames: total=%lu free=%lu\n",
            pmm_stats.total_frames, pmm_stats.free_frames);
    SAFE_PRINT("[boot] alloc_calls=%lu free_calls=%lu\n",
               pmm_stats.alloc_calls, pmm_stats.free_calls);
    for (int c = 0; c < PMM_NUM_COLORS; c++) {
        SAFE_PRINT("[boot]   color[%d] free=%lu\n", c, pmm_stats.free_by_color[c]);
    }
    arch_gdt_init();
    arch_intr_init();
    vm_init();
    lapic_init();
    arch_timer_calibrate();
    arch_timer_percpu_init();
    kinfo_init(lapic_id(), 2);
    extern uint8_t kstack_top[];
    percpu_init_this_cpu(0, lapic_id(), kstack_top);
    kprintf("[smp] BSP cpu_id=0 apic_id=%u\n", lapic_id());
    smp_start_ap();
    sched_init();

    thread_create("pager", pager_entry, stack_pager + STACK_SIZE, 18);
    thread_create("monitor", monitor_entry, stack_monitor + STACK_SIZE, 14);
    test_report_init();
    thread_create("console-in", console_in_entry, stack_console_in + STACK_SIZE, 20);
    root_task_init(untyped_base, UNTYPED_REGION_SIZE);
    devfs_init();
    ramfs_init();
    procfs_init();
    sysfs_init();
    bench_init();
    abitest_init(untyped_base, UNTYPED_REGION_SIZE);
    argvtest_init();
    panic_test_init();
    test_exit_init();
    toybox_cmd_init("true", 9);
    toybox_cmd_init("false", 9);
    toybox_cmd_init("pwd", 9);
    toybox_cmd_init("touch", 9);
    toybox_cmd_init_arg("cat", 9, "/dev/null");
    toybox_cmd_init_arg("tail", 9, "/dev/null");
    toybox_cmd_init("file", 9);
    toybox_cmd_init("find", 9);
    toybox_cmd_init("cp", 9);
    toybox_cmd_init("mv", 9);
    toybox_cmd_init("ls", 9);
    libctest_init();
    ramfstest_init();
    spawntest_init();
    consoletest_init();
    toybox_sh_c_init();

    scheduler_ready = 1;
    sched_start();
}
