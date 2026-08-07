#include "robu/types.h"
#include "robu/arch.h"
#include "robu/kprintf.h"
#include "robu/sched.h"
#include "robu/vm.h"
#include "robu/pmm.h"
#include "robu/uipc.h"
#include "robu/elf.h"
#include "robu/kinfo.h"
#include "robu/cmdline.h"
#include "robu/tar.h"
#include "robu/untyped.h"
#include "robu/captable.h"
#include "robu/devfs.h"
#include "robu/ramfs.h"
#include "robu/testreport.h"
#include "robu/rootfs.h"
#define UNTYPED_REGION_SIZE (128u * 1024)
#include "lapic.h"
#include "smp.h"
#include "percpu.h"
static uint8_t stack_server[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_client_a[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_client_b[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_spin_a[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_spin_b[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_stats[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_console_in[STACK_SIZE] __attribute__((aligned(16)));
static tid_t server_tid;
static tid_t g_sh_tid;
static volatile uint64_t spin_count_a;
static volatile uint64_t spin_count_b;
static volatile uint64_t client_rt_a;
static volatile uint64_t client_rt_b;
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static inline uint64_t irq_save(void) {
    uint64_t flags;
    asm volatile("pushfq; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}
static inline void irq_restore(uint64_t flags) {
    asm volatile("push %0; popfq" : : "r"(flags) : "memory", "cc");
}
static void console_in_entry(void) {
    for (;;) {
        int c;
        while ((c = arch_console_getc()) >= 0) {
            arch_console_line_feed(c);
        }
        ipc_sleep(1);
    }
}
int quiet_mode;
#define SAFE_PRINT(...) do {              \
        if (!quiet_mode) {                \
            uint64_t _f = irq_save();     \
            kprintf(__VA_ARGS__);         \
            irq_restore(_f);              \
        }                                 \
    } while (0)
static void server_entry(void) {
    msg_regs_t m;
    tid_t from;
    uint32_t last_cpu = (uint32_t)-1;
    uint64_t cross_core_hops = 0;
    ipc_recv(IPC_TID_ANY, &m, &from);
    for (;;) {
        m.word[0]++;
        uint32_t cpu = this_cpu()->cpu_id;
        if (last_cpu != (uint32_t)-1 && cpu != last_cpu) {
            cross_core_hops++;
            if (cross_core_hops % 50 == 0) {
                SAFE_PRINT("[echo-server] serving from cpu %u this time (was cpu %u) -- "
                           "%lu cross-core hops so far\n", cpu, last_cpu, cross_core_hops);
            }
        }
        last_cpu = cpu;
        ipc_reply_recv(from, &m, &from);
    }
}
static void client_loop(const char *tag, volatile uint64_t *rt_count) {
    msg_regs_t m;
    tid_t from;
    uint64_t n = 0;
    for (;;) {
        m.word[0] = n;
        int64_t rc = ipc_call(server_tid, &m, &from);
        if (rc != IPC_ERR_NONE || m.word[0] != n + 1) {
            SAFE_PRINT("[client %s] FAIL rc=%ld got=%lu want=%lu\n",
                       tag, rc, m.word[0], n + 1);
        }
        n++;
        *rt_count = n;
        if (n % 5000 == 0) {
            uint32_t cpu_before = this_cpu()->cpu_id;
            SAFE_PRINT("[client %s] %lu round-trips ok (last reply %lu) cpu=%u\n",
                       tag, n, m.word[0], cpu_before);
            ipc_sleep(20);
            uint32_t cpu_after = this_cpu()->cpu_id;
            if (cpu_after != cpu_before) {
                SAFE_PRINT("[client %s] woke up on a different core (cpu %u -> %u)\n",
                           tag, cpu_before, cpu_after);
            }
        }
    }
}
static void client_a_entry(void) { client_loop("A", &client_rt_a); }
static void client_b_entry(void) { client_loop("B", &client_rt_b); }
static void spinner(const char *tag, volatile uint64_t *counter) {
    for (;;) {
        uint64_t c = ++(*counter);
        if (c % 50000000 == 0) {
            SAFE_PRINT("[spin %s] alive, %lu iterations, cpu=%u\n",
                       tag, c, this_cpu()->cpu_id);
        }
    }
}
static void spinner_a_entry(void) { spinner("A", &spin_count_a); }
static void spinner_b_entry(void) { spinner("B", &spin_count_b); }
static void stats_entry(void) {
    for (;;) {
        ipc_sleep(SCHED_HZ);
        SAFE_PRINT("[stats t=%lus cpu=%u] direct=%lu sched=%lu preempt=%lu ipc=%lu "
                   "cr3(load=%lu skip=%lu) aff(hit=%lu miss=%lu) elf(parse=%lu hit=%lu) "
                   "timer(ticks=%lu traps=%lu kicks=%lu) pmm(free=%lu) "
                   "tlb(sent=%lu timeout=%lu) | "
                   "rtA=%lu rtB=%lu spinA=%lu spinB=%lu\n",
                   sched_now() / SCHED_HZ, this_cpu()->cpu_id,
                   sched_stats.direct_switches, sched_stats.full_scheds,
                   sched_stats.preempts, sched_stats.ipc_msgs,
                   vm_activate_stats.loads, vm_activate_stats.skips,
                   sched_stats.affinity_hits, sched_stats.affinity_misses,
                   elf_cache_stats.parses, elf_cache_stats.cache_hits,
                   sched_stats.ticks, sched_stats.timer_traps, sched_stats.kicks_sent,
                   pmm_stats.free_frames,
                   tlb_shootdown_stats.sent, tlb_shootdown_stats.timeouts,
                   client_rt_a, client_rt_b, spin_count_a, spin_count_b);
    }
}
#define PAGER_TID   1
#define MONITOR_TID 2
static uint8_t stack_pager[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_monitor[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_test_report[STACK_SIZE] __attribute__((aligned(16)));
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
        SAFE_PRINT("[pager] tid=%u '%s' faulted at 0x%lx -> frame 0x%lx "
                   "(color %d), mapped and resumed (#%lu)\n",
                   faulter, ft->name, fault_addr, frame, color, pager_faults_resolved);
        ipc_send(faulter, NULL);
    }
}
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
static void toybox_pipeline_advance(void);
static void toybox_pipeline_init(void);
static void ramfs_bin_seed_init(void);
static tcb_t *toybox_spawn(const char *name, int argc, const char *const *argv, uint8_t prio);
static void test_report_entry(void) {
    msg_regs_t m;
    tid_t from;
    ramfs_bin_seed_init();
    const char *starter = cmdline_get("starter");
    if (!starter) {
        starter = "sh";
    }
    const char *starter_name = starter;
    for (const char *p = starter; *p; p++) {
        if (*p == '/') {
            starter_name = p + 1;
        }
    }
    tcb_t *sh_task = toybox_spawn(starter_name, 1, (const char *const[]){ starter_name }, 9);
    g_sh_tid = sh_task->tid;
    toybox_pipeline_init();
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        switch (m.word[0]) {
        case TEST_REPORT_KIND_BENCH:
            kprintf("[bench] ipc-fast-path avg_cycles=%lu iterations=%lu (tid=%u)\n",
                    m.word[1], m.word[2], from);
            break;
        case TEST_REPORT_KIND_NOTIF_LATENCY:
            SAFE_PRINT("[bench] cross-core notify latency round=%lu elapsed_cycles=%lu "
                    "(trend only, no committed budget)\n", m.word[2], m.word[1]);
            break;
        case TEST_REPORT_KIND_GRANT_THROUGHPUT:
            SAFE_PRINT("[bench] map/grant throughput avg_cycles=%lu iterations=%lu "
                    "(trend only, no committed budget)\n", m.word[1], m.word[2]);
            break;
        case TEST_REPORT_KIND_DESTROY:
            SAFE_PRINT("[fault-injection] root-task destroy: rc=%ld redestroy_rc=%ld "
                    "(expect 0, then %d)\n",
                    (int64_t)m.word[1], (int64_t)m.word[2], IPC_ERR_NO_CAP);
            break;
        case TEST_REPORT_KIND_ARGVTEST: {
            char argv0[9] = {0};
            for (int i = 0; i < 8; i++) {
                uint8_t c = (uint8_t)(m.word[4] >> (8 * i));
                if (!c) {
                    break;
                }
                argv0[i] = (char)c;
            }
            kprintf("[argv-test] argc=%lu heap_base=0x%lx envp=0x%lx argv[0][0:8]='%s'\n",
                    m.word[1], m.word[2], m.word[3], argv0);
            break;
        }
        case TEST_REPORT_KIND_EXIT: {
            char name[17] = {0};
            for (int i = 0; i < 8; i++) {
                uint8_t c = (uint8_t)(m.word[2] >> (8 * i));
                if (!c) break;
                name[i] = (char)c;
            }
            int nlen = (int)strlen(name);
            for (int i = 0; i < 8 && nlen < 16; i++) {
                uint8_t c = (uint8_t)(m.word[3] >> (8 * i));
                if (!c) break;
                name[nlen++] = (char)c;
            }
            {
                tcb_t *exiting = sched_get_tcb(from);
                int is_interactive_child = (exiting && g_sh_tid && exiting->parent_tid == g_sh_tid)
                                          || (g_sh_tid && from == g_sh_tid);
                if (!is_interactive_child) {
                    kprintf("[toybox-exit] name='%s' status=%ld\n", name, (int64_t)m.word[1]);
                }
            }
            toybox_pipeline_advance();
            if (g_sh_tid && from == g_sh_tid) {
                int status = (int)(int64_t)m.word[1];
                kprintf("[boot] %s exited status=%d\n", name, status);
                arch_test_exit(status);
            }
            break;
        }
        case TEST_REPORT_KIND_LIBC_FDTEST:
            kprintf("[libc-fdtest] %lu/%lu checks passed (fail bitmask=0x%lx)\n",
                    m.word[2], m.word[1], m.word[3]);
            break;
        case TEST_REPORT_KIND_RAMFS_TEST:
            kprintf("[ramfs-test] %lu/%lu checks passed (fail bitmask=0x%lx)\n",
                    m.word[2], m.word[1], m.word[3]);
            break;
        case TEST_REPORT_KIND_SPAWN_TEST:
            kprintf("[spawn-test] %lu/%lu checks passed (fail bitmask=0x%lx)\n",
                    m.word[2], m.word[1], m.word[3]);
            break;
        default:
            kprintf("[test-report] unknown report kind=%lu from tid=%u\n", m.word[0], from);
            break;
        }
    }
}
#define USER_CODE_VA  0x0000000040000000ULL
#define USER_STACK_VA 0x0000000040100000ULL
extern const uint8_t user_payload_start[];
extern const uint8_t user_payload_end[];
static tid_t spawn_ring3_task(const char *name, uint8_t prio) {
    paddr_t as = vm_address_space_create();
    paddr_t code_frame = pmm_alloc(PMM_COLOR_ANY);
    uint64_t code_len = (uint64_t)(user_payload_end - user_payload_start);
    memcpy((void *)code_frame, user_payload_start, code_len);
    arch_vm_map_page(as, USER_CODE_VA, code_frame,
                     VM_PROT_READ | VM_PROT_EXEC | VM_PROT_USER);
    paddr_t stack_frame = pmm_alloc(PMM_COLOR_ANY);
    arch_vm_map_page(as, USER_STACK_VA, stack_frame,
                     VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER);
    tcb_t *t = thread_create_user(name, USER_CODE_VA, USER_STACK_VA + PAGE_SIZE_4K,
                                  prio, as, PAGER_TID);
    return t->tid;
}
#define CLIENT_VGA_VA 0x0000000040300000ULL
#define VGA_MMIO_PADDR 0xB8000ULL
static uint8_t stack_mmio_driver[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_mmio_intruder[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_mmio_client[STACK_SIZE] __attribute__((aligned(16)));
static tid_t mmio_driver_tid, mmio_intruder_tid;
static void mmio_driver_entry(void) {
    msg_regs_t hello;
    tid_t from;
    ipc_recv(IPC_TID_ANY, &hello, &from);
    vm_map_msg_t req = { CLIENT_VGA_VA, VGA_MMIO_PADDR,
                         VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER, 1 };
    int64_t rc = robu_ipc_raw(from, 0, IPC_FLAG_MAP, (msg_regs_t *)&req, NULL);
    SAFE_PRINT("[mmio] driver (holds CAP_PERM_MAP) mapped VGA for tid=%u -> rc=%ld (0=ok)\n",
               from, rc);
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}
static void mmio_intruder_entry(void) {
    msg_regs_t hello;
    tid_t from;
    ipc_recv(IPC_TID_ANY, &hello, &from);
    vm_map_msg_t req = { CLIENT_VGA_VA, VGA_MMIO_PADDR,
                         VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER, 1 };
    int64_t rc = robu_ipc_raw(from, 0, IPC_FLAG_MAP, (msg_regs_t *)&req, NULL);
    SAFE_PRINT("[mmio] intruder (NO capability) attempted the same VGA map for "
               "tid=%u -> rc=%ld (expect %d=denied)\n",
               from, rc, IPC_ERR_NO_CAP);
    msg_regs_t retype_attempt;
    retype_attempt.word[0] = 0;
    retype_attempt.word[1] = CAP_KIND_FRAME;
    retype_attempt.word[2] = 0;
    retype_attempt.word[3] = 0;
    retype_attempt.word[4] = 0;
    retype_attempt.word[5] = 0;
    int64_t retype_rc = robu_ipc_raw(0, 0, IPC_FLAG_RETYPE, &retype_attempt, NULL);
    SAFE_PRINT("[captable] intruder (owns no capability) attempted to retype "
               "another task's Untyped cap (slot 0) -> rc=%ld (expect %d=denied)\n",
               retype_rc, IPC_ERR_NO_CAP);
    ipc_sleep(SCHED_HZ);
    msg_regs_t destroy_attempt;
    destroy_attempt.word[0] = 3;
    destroy_attempt.word[1] = 0;
    destroy_attempt.word[2] = 0;
    destroy_attempt.word[3] = 0;
    destroy_attempt.word[4] = 0;
    destroy_attempt.word[5] = 0;
    int64_t destroy_rc = robu_ipc_raw(0, 0, IPC_FLAG_DESTROY, &destroy_attempt, NULL);
    SAFE_PRINT("[captable] intruder (owns no capability) attempted to destroy "
               "another task's child TCB cap (slot 3) -> rc=%ld (expect %d=denied)\n",
               destroy_rc, IPC_ERR_NO_CAP);
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}
static void mmio_client_entry(void) {
    msg_regs_t hello = {0};
    msg_regs_t ack;
    tid_t from;
    ipc_send(mmio_driver_tid, &hello);
    ipc_recv(mmio_driver_tid, &ack, &from);
    ipc_send(mmio_intruder_tid, &hello);
    ipc_recv(mmio_intruder_tid, &ack, &from);
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}
#define XFER_VA 0x0000000040400000ULL
static uint8_t stack_xfer_producer[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_xfer_consumer[STACK_SIZE] __attribute__((aligned(16)));
static tid_t xfer_consumer_tid;
static void xfer_producer_entry(void) {
    paddr_t frame = pmm_alloc(PMM_COLOR_ANY);
    uint64_t *mem = (uint64_t *)frame;
    for (int i = 0; i < 8; i++) {
        mem[i] = 0xC0FFEE0000000000ULL | (uint64_t)i;
    }
    arch_vm_map_page(current_thread->address_space, XFER_VA, frame,
                     VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER);
    ipc_sleep(SCHED_HZ / 2);
    vm_xfer_msg_t xfer = { XFER_VA, XFER_VA,
                          VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER, 1 };
    int64_t rc = robu_ipc_raw(xfer_consumer_tid, 0, IPC_FLAG_XFER,
                              (msg_regs_t *)&xfer, NULL);
    SAFE_PRINT("[xfer] producer transferred its page to tid=%u -> rc=%ld (0=ok)\n",
               xfer_consumer_tid, rc);
    volatile uint64_t *old = (volatile uint64_t *)XFER_VA;
    *old = 0xDEAD0000;
    SAFE_PRINT("[xfer] producer touched its old VA after giving it away "
               "(the pager line above is the proof it was really unmapped)\n");
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}
static void xfer_consumer_entry(void) {
    msg_regs_t m;
    tid_t from;
    ipc_recv(IPC_TID_ANY, &m, &from);
    volatile uint64_t *mem = (volatile uint64_t *)XFER_VA;
    SAFE_PRINT("[xfer] consumer received the page from tid=%u, word0=0x%lx "
               "(expect 0xc0ffee...)\n", from, mem[0]);
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}
extern const uint8_t server_payload_start[];
extern const uint8_t server_payload_end[];
static uint8_t stack_supervisor[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_chaos_client[STACK_SIZE] __attribute__((aligned(16)));
static volatile tid_t crash_server_tid;
static volatile uint32_t crash_server_generation;
static tid_t spawn_crash_server(void) {
    paddr_t as = vm_address_space_create();
    paddr_t code_frame = pmm_alloc(PMM_COLOR_ANY);
    uint64_t code_len = (uint64_t)(server_payload_end - server_payload_start);
    memcpy((void *)code_frame, server_payload_start, code_len);
    arch_vm_map_page(as, USER_CODE_VA, code_frame,
                     VM_PROT_READ | VM_PROT_EXEC | VM_PROT_USER);
    paddr_t stack_frame = pmm_alloc(PMM_COLOR_ANY);
    arch_vm_map_page(as, USER_STACK_VA, stack_frame,
                     VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER);
    tcb_t *t = thread_create_user("crash-server", USER_CODE_VA,
                                  USER_STACK_VA + PAGE_SIZE_4K, 7, as, PAGER_TID);
    crash_server_generation++;
    SAFE_PRINT("[supervisor] crash-server generation %u is tid=%u\n",
               crash_server_generation, t->tid);
    return t->tid;
}
static void supervisor_entry(void) {
    crash_server_tid = spawn_crash_server();
    for (;;) {
        ipc_sleep(SCHED_HZ / 2);
        if (!sched_get_tcb(crash_server_tid)) {
            SAFE_PRINT("[supervisor] tid=%u is gone -- restarting the service\n",
                       crash_server_tid);
            crash_server_tid = spawn_crash_server();
        }
    }
}
static void chaos_client_entry(void) {
    msg_regs_t m;
    tid_t from;
    uint64_t n = 0;
    uint64_t crashes_seen = 0;
    for (;;) {
        n++;
        m.word[0] = (n % 15 == 0) ? 0xDEAD : n;
        int64_t rc = ipc_call(crash_server_tid, &m, &from);
        if (rc == IPC_ERR_NOT_FOUND) {
            crashes_seen++;
            SAFE_PRINT("[chaos-client] request #%lu to tid=%u -> IPC_ERR_NOT_FOUND "
                       "(the service just crashed; will retry against whatever "
                       "the supervisor respawns) [%lu total]\n",
                       n, crash_server_tid, crashes_seen);
        } else if (n % 15 != 0 && (rc != IPC_ERR_NONE || m.word[0] != n + 1)) {
            SAFE_PRINT("[chaos-client] FAIL request #%lu rc=%ld got=%lu\n",
                       n, rc, m.word[0]);
        }
        ipc_sleep(SCHED_HZ / 5);
    }
}
#define SHM_VA 0x0000000040500000ULL
static uint8_t stack_shm_writer[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_shm_reader[STACK_SIZE] __attribute__((aligned(16)));
static tid_t shm_reader_tid;
static void shm_writer_entry(void) {
    paddr_t frame = pmm_alloc(PMM_COLOR_ANY);
    uint64_t *mem = (uint64_t *)frame;
    mem[0] = 0;
    arch_vm_map_page(current_thread->address_space, SHM_VA, frame,
                     VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER);
    ipc_sleep(SCHED_HZ / 2);
    vm_xfer_msg_t share = { SHM_VA, SHM_VA,
                           VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER, 1 };
    int64_t rc = robu_ipc_raw(shm_reader_tid, 0, IPC_FLAG_SHARE,
                              (msg_regs_t *)&share, NULL);
    SAFE_PRINT("[shm] writer shared its page with tid=%u -> rc=%ld (0=ok); "
               "no further IPC needed for either side to see updates\n",
               shm_reader_tid, rc);
    volatile uint64_t *counter = (volatile uint64_t *)SHM_VA;
    for (;;) {
        (*counter)++;
        ipc_sleep(SCHED_HZ / 4);
    }
}
static void shm_reader_entry(void) {
    msg_regs_t m;
    tid_t from;
    ipc_recv(IPC_TID_ANY, &m, &from);
    volatile uint64_t *counter = (volatile uint64_t *)SHM_VA;
    for (;;) {
        ipc_sleep(SCHED_HZ / 2);
        SAFE_PRINT("[shm] reader sees live counter=%lu through its own mapping "
                   "of the same physical page\n", *counter);
    }
}
#define BATCH_VA 0x0000000040600000ULL
#define BATCH_ITEMS 8
static uint8_t stack_batch_server[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_batch_client[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_share_bench_target[STACK_SIZE] __attribute__((aligned(16)));
static tid_t batch_server_tid;
static tid_t share_bench_target_tid;
static void share_bench_target_entry(void) {
    msg_regs_t m;
    tid_t from;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
    }
}
static void batch_server_entry(void) {
    msg_regs_t m;
    tid_t from;
    ipc_recv(IPC_TID_ANY, &m, &from);
    volatile uint64_t *buf = (volatile uint64_t *)BATCH_VA;
    for (;;) {
        ipc_recv(IPC_TID_ANY, &m, &from);
        uint64_t count = m.word[0];
        uint64_t sum = 0;
        for (uint64_t i = 0; i < count; i++) {
            buf[i] = buf[i] * 2;
            sum += buf[i];
        }
        m.word[0] = count;
        m.word[1] = sum;
        ipc_send(from, &m);
    }
}
static void batch_client_entry(void) {
    paddr_t frame = pmm_alloc(PMM_COLOR_ANY);
    uint64_t *mem = (uint64_t *)frame;
    for (int i = 0; i < BATCH_ITEMS; i++) {
        mem[i] = (uint64_t)(i + 1);
    }
    arch_vm_map_page(current_thread->address_space, BATCH_VA, frame,
                     VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER);
    ipc_sleep(SCHED_HZ / 2);
    vm_xfer_msg_t share = { BATCH_VA, BATCH_VA,
                           VM_PROT_READ | VM_PROT_WRITE | VM_PROT_USER, 1 };
    robu_ipc_raw(batch_server_tid, 0, IPC_FLAG_SHARE, (msg_regs_t *)&share, NULL);
    {
        uint64_t t0 = rdtsc();
        for (uint64_t i = 0; i < 20000; i++) {
            robu_ipc_raw(share_bench_target_tid, 0, IPC_FLAG_SHARE, (msg_regs_t *)&share, NULL);
        }
        uint64_t t1 = rdtsc();
        uint64_t avg_cycles = (t1 - t0) / 20000;
        msg_regs_t r = (msg_regs_t){0};
        r.word[0] = TEST_REPORT_KIND_GRANT_THROUGHPUT;
        r.word[1] = avg_cycles;
        r.word[2] = 20000;
        ipc_send(kinfo_user()->test_report_tid, &r);
    }
    for (uint64_t round = 0;; round++) {
        msg_regs_t m = {0};
        m.word[0] = BATCH_ITEMS;
        tid_t from;
        int64_t rc = ipc_call(batch_server_tid, &m, &from);
        SAFE_PRINT("[batch] round %lu: %lu items doubled in ONE message, "
                   "sum=%lu (rc=%ld)\n", round, m.word[0], m.word[1], rc);
        ipc_sleep(SCHED_HZ * 2);
    }
}
#define ASYNC_CLIENTS 3
static uint8_t stack_async_server[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_async_client[ASYNC_CLIENTS][STACK_SIZE] __attribute__((aligned(16)));
static tid_t async_server_tid;
static void async_server_entry(void) {
    msg_regs_t m;
    tid_t from;
    uint64_t serviced = 0;
    uint64_t idle_polls = 0;
    for (;;) {
        int64_t rc = robu_ipc_raw(0, IPC_TID_ANY, IPC_FLAG_RECV | IPC_FLAG_NOBLOCK,
                                  (msg_regs_t *)&m, &from);
        if (rc == IPC_ERR_NONE) {
            serviced++;
            m.word[0] = m.word[0] * 10;
            ipc_send(from, &m);
        } else {
            idle_polls++;
            if (idle_polls % 100 == 0) {
                SAFE_PRINT("[async-server] idle housekeeping tick -- %lu requests "
                           "serviced so far from %d clients, all through this one "
                           "thread\n", serviced, ASYNC_CLIENTS);
            }
            ipc_sleep(SCHED_HZ / 50);
        }
    }
}
static void async_client_loop(int idx) {
    msg_regs_t m;
    tid_t from;
    uint64_t n = 0;
    for (;;) {
        n++;
        m.word[0] = n;
        int64_t rc = ipc_call(async_server_tid, &m, &from);
        if (rc != IPC_ERR_NONE || m.word[0] != n * 10) {
            SAFE_PRINT("[async-client %d] FAIL rc=%ld got=%lu want=%lu\n",
                       idx, rc, m.word[0], n * 10);
        }
        ipc_sleep(SCHED_HZ / 3 + (uint64_t)idx * 7);
    }
}
static void async_client_0_entry(void) { async_client_loop(0); }
static void async_client_1_entry(void) { async_client_loop(1); }
static void async_client_2_entry(void) { async_client_loop(2); }
#define SANDBOX_SIZE 4096
static uint8_t sandbox_region[SANDBOX_SIZE] __attribute__((aligned(16)));
static uint8_t stack_sfi_domain_a[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_sfi_domain_b[STACK_SIZE] __attribute__((aligned(16)));
static int sandbox_write(uint32_t offset, uint8_t value) {
    if (offset >= SANDBOX_SIZE) {
        return -1;
    }
    sandbox_region[offset] = value;
    return 0;
}
static int sandbox_read(uint32_t offset, uint8_t *out) {
    if (offset >= SANDBOX_SIZE) {
        return -1;
    }
    *out = sandbox_region[offset];
    return 0;
}
static void sfi_domain_a_entry(void) {
    for (uint32_t i = 0; i < SANDBOX_SIZE; i++) {
        sandbox_write(i, (uint8_t)(i & 0xFF));
    }
    SAFE_PRINT("[sfi] domain-A filled the shared sandbox through the bounds-"
               "checked gate (as=0x%lx)\n", current_thread->address_space);
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}
static void sfi_domain_b_entry(void) {
    ipc_sleep(SCHED_HZ / 4);
    uint8_t v = 0;
    int rc_ok = sandbox_read(10, &v);
    int rc_oob = sandbox_write(SANDBOX_SIZE + 100, 0xFF);
    SAFE_PRINT("[sfi] domain-B (as=0x%lx, same as domain-A -- no MMU boundary "
               "between them) in-bounds read [10]=%u (rc=%d); out-of-bounds "
               "write denied by the gate itself, no hardware fault involved "
               "(rc=%d)\n", current_thread->address_space, v, rc_ok, rc_oob);
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}
static const uint8_t *service_elf_start, *service_elf_end;
static const uint8_t *task_elf_start, *task_elf_end;
#define ROOTFS_MAX_SIZE (1536 * 1024)
static uint8_t rootfs_buf[ROOTFS_MAX_SIZE];
static uint64_t rootfs_len;
int rootfs_lookup(const char *name, const uint8_t **out_start, const uint8_t **out_end) {
    const uint8_t *start;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, name, &start, &sz) != 0) {
        return -1;
    }
    *out_start = start;
    *out_end = start + sz;
    return 0;
}
static const char *rootfs_split_pair(char *buf, const char **second) {
    for (char *p = buf; *p; p++) {
        if (*p == ',') {
            *p = '\0';
            *second = p + 1;
            return buf;
        }
    }
    *second = "";
    return buf;
}
static void rootfs_init(void) {
    cmdline_parse(arch_boot_cmdline());
    paddr_t mod_base;
    uint64_t mod_len;
    if (arch_boot_module(&mod_base, &mod_len) != 0) {
        kprintf("[boot] FATAL: no boot module supplied -- pass -initrd <rootfs.tar>\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    if (mod_len > sizeof(rootfs_buf)) {
        kprintf("[boot] FATAL: rootfs module is %lu bytes, exceeds the %lu byte buffer\n",
                mod_len, (uint64_t)sizeof(rootfs_buf));
        for (;;) { asm volatile("cli; hlt"); }
    }
    memcpy(rootfs_buf, (const void *)mod_base, mod_len);
    rootfs_len = mod_len;
    kprintf("[boot] rootfs: %lu bytes copied from module at 0x%lx\n", mod_len, (uint64_t)mod_base);
    static char apps_buf[128];
    const char *apps = cmdline_get("apps");
    if (!apps) {
        apps = "hello_service,hello_task";
    }
    size_t len = strlen(apps);
    if (len >= sizeof(apps_buf)) {
        len = sizeof(apps_buf) - 1;
    }
    memcpy(apps_buf, apps, len);
    apps_buf[len] = '\0';
    const char *task_name;
    const char *service_name = rootfs_split_pair(apps_buf, &task_name);
    uint64_t sz;
    if (tar_find(rootfs_buf, mod_len, service_name, &service_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named '%s'\n", service_name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    service_elf_end = service_elf_start + sz;
    if (tar_find(rootfs_buf, mod_len, task_name, &task_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named '%s'\n", task_name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    task_elf_end = task_elf_start + sz;
    kprintf("[boot] loaded service='%s' task='%s'\n", service_name, task_name);
}
static void root_task_init(paddr_t untyped_base, uint64_t untyped_size) {
    const char *root_name = cmdline_get("root");
    if (!root_name) {
        root_name = "root_task";
    }
    const uint8_t *root_elf_start, *root_elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, root_name, &root_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named '%s'\n", root_name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    root_elf_end = root_elf_start + sz;
    tcb_t *root = elf_load_and_spawn("root-task", root_elf_start, root_elf_end, 13, PAGER_TID);
    if (!root) {
        kprintf("[boot] FATAL: root task failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kcap_grant(root->tid, CAP_KIND_UNTYPED, (uint64_t)untyped_base, untyped_size);
    kprintf("[boot] root task: tid=%u\n", root->tid);
}
static tid_t devfs_init(void) {
    const char *devfs_name = cmdline_get("devfs");
    if (!devfs_name) {
        devfs_name = "devfs";
    }
    const uint8_t *devfs_elf_start, *devfs_elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, devfs_name, &devfs_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named '%s'\n", devfs_name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    devfs_elf_end = devfs_elf_start + sz;
    tcb_t *devfs = elf_load_and_spawn("devfs", devfs_elf_start, devfs_elf_end, 11, PAGER_TID);
    if (!devfs) {
        kprintf("[boot] FATAL: devfs server failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    ipc_grant_console_writer(devfs->tid);
    kinfo_set_devfs_tid(devfs->tid);
    kprintf("[boot] devfs server: tid=%u, granted console-write permission\n", devfs->tid);
    return devfs->tid;
}
static tid_t ramfs_init(void) {
    const uint8_t *ramfs_elf_start, *ramfs_elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, "ramfs", &ramfs_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'ramfs'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    ramfs_elf_end = ramfs_elf_start + sz;
    tcb_t *ramfs = elf_load_and_spawn("ramfs", ramfs_elf_start, ramfs_elf_end, 11, PAGER_TID);
    if (!ramfs) {
        kprintf("[boot] FATAL: ramfs server failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kinfo_set_ramfs_tid(ramfs->tid);
    kprintf("[boot] ramfs server: tid=%u\n", ramfs->tid);
    return ramfs->tid;
}
static tid_t procfs_init(void) {
    const uint8_t *procfs_elf_start, *procfs_elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, "procfs", &procfs_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'procfs'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    procfs_elf_end = procfs_elf_start + sz;
    tcb_t *procfs = elf_load_and_spawn("procfs", procfs_elf_start, procfs_elf_end, 11, PAGER_TID);
    if (!procfs) {
        kprintf("[boot] FATAL: procfs server failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kinfo_set_procfs_tid(procfs->tid);
    kprintf("[boot] procfs server: tid=%u\n", procfs->tid);
    return procfs->tid;
}
static tid_t sysfs_init(void) {
    const uint8_t *sysfs_elf_start, *sysfs_elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, "sysfs", &sysfs_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'sysfs'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    sysfs_elf_end = sysfs_elf_start + sz;
    tcb_t *sysfs = elf_load_and_spawn("sysfs", sysfs_elf_start, sysfs_elf_end, 11, PAGER_TID);
    if (!sysfs) {
        kprintf("[boot] FATAL: sysfs server failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kinfo_set_sysfs_tid(sysfs->tid);
    kprintf("[boot] sysfs server: tid=%u\n", sysfs->tid);
    return sysfs->tid;
}
static void bench_init(void) {
    if (!cmdline_get("bench")) {
        return;
    }
    const uint8_t *server_elf_start, *server_elf_end;
    const uint8_t *client_elf_start, *client_elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, "benchserver", &server_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'benchserver'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    server_elf_end = server_elf_start + sz;
    if (tar_find(rootfs_buf, rootfs_len, "benchclient", &client_elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'benchclient'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    client_elf_end = client_elf_start + sz;
    tcb_t *server = elf_load_and_spawn("bench-server", server_elf_start, server_elf_end, 10, PAGER_TID);
    tcb_t *client = elf_load_and_spawn("bench-client", client_elf_start, client_elf_end, 10, PAGER_TID);
    if (!server || !client) {
        kprintf("[boot] FATAL: bench client/server failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kinfo_set_benchserver_tid(server->tid);
    kprintf("[boot] bench: server tid=%u, client tid=%u\n", server->tid, client->tid);
}
static uint8_t stack_abitest_helper[STACK_SIZE] __attribute__((aligned(16)));
static void abitest_helper_entry(void) {
    for (;;) {
        ipc_sleep(1000000);
    }
}
static uint8_t stack_abitest_exit_helper[STACK_SIZE] __attribute__((aligned(16)));
static void abitest_exit_helper_entry(void) {
    for (;;) {
        msg_regs_t m;
        tid_t from;
        ipc_recv(IPC_TID_ANY, &m, &from);
        msg_regs_t probe = (msg_regs_t){0};
        int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_EXIT, &probe, NULL);
        m.word[0] = (uint64_t)rc;
        ipc_send(from, &m);
    }
}
static void abitest_init(paddr_t untyped_base, uint64_t untyped_size) {
    if (!cmdline_get("abitest")) {
        return;
    }
    const uint8_t *elf_start, *elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, "abitest", &elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'abitest'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    elf_end = elf_start + sz;
    tcb_t *helper = thread_create("abitest-helper", abitest_helper_entry,
                                  stack_abitest_helper + STACK_SIZE, 5);
    kinfo_set_abitest_helper_tid(helper->tid);
    tcb_t *exit_helper = thread_create("abitest-exit-helper", abitest_exit_helper_entry,
                                       stack_abitest_exit_helper + STACK_SIZE, 5);
    kinfo_set_abitest_exit_helper_tid(exit_helper->tid);
    tcb_t *abitest = elf_load_and_spawn("abitest", elf_start, elf_end, 14, PAGER_TID);
    if (!abitest) {
        kprintf("[boot] FATAL: abitest failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    uint64_t untyped_slot = kcap_next_slot();
    kcap_grant(abitest->tid, CAP_KIND_UNTYPED, (uint64_t)untyped_base, untyped_size);
    uint64_t rt_slot, rt_addr;
    cap_retype(abitest->tid, (uint32_t)untyped_slot, CAP_KIND_NOTIFICATION,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    uint64_t notif_slot = rt_slot;
    cap_retype(abitest->tid, (uint32_t)untyped_slot, CAP_KIND_TIMER,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    uint64_t timer_slot = rt_slot;
    cap_retype(abitest->tid, (uint32_t)untyped_slot, CAP_KIND_FRAME,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    uint64_t revoke_frame_slot = rt_slot;
    uint64_t helper_tcb_slot = kcap_next_slot();
    kcap_grant(abitest->tid, CAP_KIND_TCB, helper->tid, 0);
    kinfo_set_abitest_slots((uint32_t)untyped_slot, (uint32_t)notif_slot,
                            (uint32_t)timer_slot, (uint32_t)helper_tcb_slot,
                            (uint32_t)revoke_frame_slot);
    kprintf("[boot] abitest: tid=%u, helper tid=%u\n", abitest->tid, helper->tid);
}
static void argvtest_init(void) {
    if (!cmdline_get("argvtest")) {
        return;
    }
    const uint8_t *elf_start, *elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, "argvtest", &elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'argvtest'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    elf_end = elf_start + sz;
    static const char *const argv[] = { "argvtest", "hello" };
    tcb_t *t = elf_load_and_spawn_argv("argvtest", elf_start, elf_end, 9, PAGER_TID,
                                       2, argv);
    if (!t) {
        kprintf("[boot] FATAL: argvtest failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kprintf("[boot] argvtest: tid=%u\n", t->tid);
}
static uint8_t stack_panic_test[STACK_SIZE] __attribute__((aligned(16)));
static void __attribute__((noinline)) panic_test_fault(void) {
    asm volatile("ud2");
}
static void panic_test_entry(void) {
    kputs("[boot] deliberately triggering a kernel-mode fault...");
    panic_test_fault();
}
static void panic_test_init(void) {
    if (!cmdline_get("panic_test")) {
        return;
    }
    thread_create("panic-test", panic_test_entry, stack_panic_test + STACK_SIZE, 9);
}
static uint8_t stack_test_exit[STACK_SIZE] __attribute__((aligned(16)));
static int test_exit_code;
static void test_exit_entry(void) {
    ipc_sleep(SCHED_HZ * 3);
    kprintf("[boot] exiting with code %d\n", test_exit_code);
    arch_test_exit(test_exit_code);
}
static void test_exit_init(void) {
    const char *val = cmdline_get("test_exit");
    if (!val) {
        return;
    }
    int code = 0;
    while (*val >= '0' && *val <= '9') {
        code = code * 10 + (*val - '0');
        val++;
    }
    if (code > 255) {
        code = 255;
    }
    test_exit_code = code;
    thread_create("test-exit", test_exit_entry, stack_test_exit + STACK_SIZE, 9);
}
static tcb_t *toybox_spawn(const char *name, int argc, const char *const *argv, uint8_t prio) {
    const uint8_t *elf_start, *elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, name, &elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named '%s'\n", name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    elf_end = elf_start + sz;
    tcb_t *t = elf_load_and_spawn_argv(name, elf_start, elf_end, prio, PAGER_TID, argc, argv);
    if (!t) {
        kprintf("[boot] FATAL: %s failed to load\n", name);
        for (;;) { asm volatile("cli; hlt"); }
    }
    kprintf("[boot] toybox %s: tid=%u\n", name, t->tid);
    return t;
}
static void toybox_cmd_init_arg(const char *name, uint8_t prio, const char *arg1) {
    char flag[32] = "toybox_";
    size_t i = 0;
    while (name[i] && i < sizeof(flag) - 8) {
        flag[7 + i] = name[i];
        i++;
    }
    flag[7 + i] = '\0';
    if (!cmdline_get(flag)) {
        return;
    }
    if (arg1) {
        const char *argv[2];
        argv[0] = name;
        argv[1] = arg1;
        toybox_spawn(name, 2, argv, prio);
        return;
    }
    const char *argv[1];
    argv[0] = name;
    toybox_spawn(name, 1, argv, prio);
}
static void toybox_cmd_init(const char *name, uint8_t prio) {
    toybox_cmd_init_arg(name, prio, NULL);
}
typedef struct {
    const char *name;
    const char *argv[4];
    int argc;
} pipeline_step_t;
static const pipeline_step_t toybox_pipeline_steps[] = {
    { "touch", { "touch", "/touched.txt" }, 2 },
    { "cat",   { "cat", "/greeting.txt" }, 2 },
    { "ls",    { "ls", "/" }, 2 },
    { "cp",    { "cp", "/greeting.txt", "/copy.txt" }, 3 },
    { "mv",    { "mv", "/copy.txt", "/renamed.txt" }, 3 },
    { "tail",  { "tail", "/renamed.txt" }, 2 },
    { "find",  { "find", "/", "-name", "renamed.txt" }, 4 },
};
#define TOYBOX_PIPELINE_STEP_COUNT \
    (sizeof(toybox_pipeline_steps) / sizeof(toybox_pipeline_steps[0]))
static int toybox_pipeline_active;
static uint64_t toybox_pipeline_index;
static void toybox_pipeline_advance(void) {
    if (!toybox_pipeline_active) {
        return;
    }
    if (toybox_pipeline_index >= TOYBOX_PIPELINE_STEP_COUNT) {
        kprintf("[toybox-pipeline] all %lu step(s) complete\n",
                (uint64_t)TOYBOX_PIPELINE_STEP_COUNT);
        toybox_pipeline_active = 0;
        return;
    }
    const pipeline_step_t *step = &toybox_pipeline_steps[toybox_pipeline_index];
    toybox_spawn(step->name, step->argc, step->argv, 9);
    toybox_pipeline_index++;
}
static void toybox_pipeline_init(void) {
    if (!cmdline_get("toybox_pipeline")) {
        return;
    }
    static const char greeting[] = "hello from the toybox port\n";
    int64_t h = ramfs_open("greeting.txt", RAMFS_O_CREAT | RAMFS_O_TRUNC);
    if (h < 0) {
        kprintf("[boot] FATAL: could not seed /greeting.txt in ramfs\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    ramfs_write((uint64_t)h, greeting, sizeof(greeting) - 1);
    ramfs_close((uint64_t)h);
    toybox_pipeline_active = 1;
    toybox_pipeline_index = 0;
    toybox_pipeline_advance();
}
static void ramfs_bin_seed_init(void) {
    static const char *const names[] = {
        "sh", "ls", "cat", "touch", "tail", "file", "find", "cp", "mv"
    };
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        const uint8_t *start;
        uint64_t sz;
        if (tar_find(rootfs_buf, rootfs_len, names[i], &start, &sz) != 0) {
            kprintf("[boot] /bin seed: FATAL: rootfs has no entry named '%s'\n", names[i]);
            for (;;) { asm volatile("cli; hlt"); }
        }
        char path[RAMFS_NAME_MAX];
        uint32_t p = 0;
        static const char prefix[] = "bin/";
        for (uint32_t j = 0; prefix[j] && p < sizeof(path) - 1; j++) {
            path[p++] = prefix[j];
        }
        for (uint32_t j = 0; names[i][j] && p < sizeof(path) - 1; j++) {
            path[p++] = names[i][j];
        }
        path[p] = '\0';
        int64_t h = ramfs_open(path, RAMFS_O_CREAT | RAMFS_O_TRUNC);
        if (h < 0) {
            kprintf("[boot] /bin seed: FATAL: ramfs_open('%s') failed rc=%ld\n", path, h);
            for (;;) { asm volatile("cli; hlt"); }
        }
        uint64_t off = 0;
        while (off < sz) {
            uint64_t chunk = sz - off;
            if (chunk > RAMFS_WRITE_MAX) {
                chunk = RAMFS_WRITE_MAX;
            }
            int64_t n = ramfs_write((uint64_t)h, start + off, chunk);
            if (n <= 0) {
                kprintf("[boot] /bin seed: FATAL: ramfs_write('%s') failed at off=%lu\n",
                        path, off);
                for (;;) { asm volatile("cli; hlt"); }
            }
            off += (uint64_t)n;
        }
        ramfs_close((uint64_t)h);
    }
    kprintf("[boot] /bin seed: %lu real binaries copied into ramfs\n",
            (uint64_t)(sizeof(names) / sizeof(names[0])));
}
static void libctest_init(void) {
    if (!cmdline_get("libctest")) {
        return;
    }
    const uint8_t *elf_start, *elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, "libctest", &elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'libctest'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    elf_end = elf_start + sz;
    static const char *const argv[] = { "libctest" };
    tcb_t *t = elf_load_and_spawn_argv("libctest", elf_start, elf_end, 9, PAGER_TID, 1, argv);
    if (!t) {
        kprintf("[boot] FATAL: libctest failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kprintf("[boot] libctest: tid=%u\n", t->tid);
}
static void ramfstest_init(void) {
    if (!cmdline_get("ramfstest")) {
        return;
    }
    const uint8_t *elf_start, *elf_end;
    uint64_t sz;
    if (tar_find(rootfs_buf, rootfs_len, "ramfstest", &elf_start, &sz) != 0) {
        kprintf("[boot] FATAL: rootfs has no entry named 'ramfstest'\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    elf_end = elf_start + sz;
    static const char *const argv[] = { "ramfstest" };
    tcb_t *t = elf_load_and_spawn_argv("ramfstest", elf_start, elf_end, 9, PAGER_TID, 1, argv);
    if (!t) {
        kprintf("[boot] FATAL: ramfstest failed to load\n");
        for (;;) { asm volatile("cli; hlt"); }
    }
    kprintf("[boot] ramfstest: tid=%u\n", t->tid);
}
static void spawntest_init(void) {
    if (!cmdline_get("spawntest")) {
        return;
    }
    toybox_spawn("spawntest", 1, (const char *const[]){ "spawntest" }, 9);
}
static void consoletest_init(void) {
    if (!cmdline_get("consoletest")) {
        return;
    }
    toybox_spawn("consoletest", 1, (const char *const[]){ "consoletest" }, 9);
}
static void toybox_sh_c_init(void) {
    const char *cmd = cmdline_get("toybox_sh_c");
    if (!cmd) {
        return;
    }
    toybox_spawn("sh", 3, (const char *const[]){ "sh", "-c", cmd }, 9);
}
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
static uint8_t stack_timer_demo[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_timer_demo_helper[STACK_SIZE] __attribute__((aligned(16)));
static uint64_t timer_demo_untyped_slot;
static uint64_t timer_demo_notif_slot;
static uint64_t timer_demo_notif_idx;
static uint64_t timer_demo_timer_slot;
static uint64_t timer_demo_helper_notif_slot;
static void timer_demo_helper_entry(void) {
    msg_regs_t m = {0};
    m.word[0] = timer_demo_helper_notif_slot;
    robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_WAIT, &m, NULL);
    for (;;) {
        ipc_sleep(SCHED_HZ * 1000);
    }
}
static void timer_demo_entry(void) {
    msg_regs_t m;
    int64_t rc;
    uint64_t notif_slot = timer_demo_notif_slot;
    uint64_t notif_idx = timer_demo_notif_idx;
    uint64_t timer_slot = timer_demo_timer_slot;
    SAFE_PRINT("[timer-demo] notification slot=%lu, timer slot=%lu (minted at boot)\n",
            notif_slot, timer_slot);
    m = (msg_regs_t){0};
    m.word[0] = notif_slot;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_POLL, &m, NULL);
    SAFE_PRINT("[timer-demo] poll (fresh) -> rc=%ld (expect %d=wouldblock)\n", rc, IPC_ERR_WOULDBLOCK);
    uint64_t t0 = sched_now();
    m = (msg_regs_t){0};
    m.word[0] = timer_slot;
    m.word[1] = notif_slot;
    m.word[2] = 20;
    m.word[3] = 0xABCD;
    m.word[4] = 0;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_TIMER_ARM, &m, NULL);
    SAFE_PRINT("[timer-demo] arm one-shot, 20 ticks out -> rc=%ld\n", rc);
    m = (msg_regs_t){0};
    m.word[0] = notif_slot;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_WAIT, &m, NULL);
    uint64_t elapsed = sched_now() - t0;
    SAFE_PRINT("[timer-demo] wait -> rc=%ld bits=0x%lx elapsed=%lu ticks (expect ~20, bits=0xabcd)\n",
            rc, m.word[0], elapsed);
    m = (msg_regs_t){0};
    m.word[0] = timer_slot;
    m.word[1] = notif_slot;
    m.word[2] = 10;
    m.word[3] = 0x1;
    m.word[4] = 10;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_TIMER_ARM, &m, NULL);
    SAFE_PRINT("[timer-demo] arm periodic, every 10 ticks -> rc=%ld\n", rc);
    for (int i = 0; i < 3; i++) {
        m = (msg_regs_t){0};
        m.word[0] = notif_slot;
        rc = robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_WAIT, &m, NULL);
        SAFE_PRINT("[timer-demo] periodic wait #%d -> rc=%ld bits=0x%lx\n", i, rc, m.word[0]);
    }
    m = (msg_regs_t){0};
    m.word[0] = timer_slot;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_TIMER_DISARM, &m, NULL);
    SAFE_PRINT("[timer-demo] disarm periodic -> rc=%ld\n", rc);
    m = (msg_regs_t){0};
    m.word[0] = timer_slot;
    m.word[1] = notif_slot;
    m.word[2] = 30;
    m.word[3] = 0xDEAD;
    m.word[4] = 0;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_TIMER_ARM, &m, NULL);
    SAFE_PRINT("[timer-demo] arm one-shot, 30 ticks out (to be disarmed) -> rc=%ld\n", rc);
    m = (msg_regs_t){0};
    m.word[0] = timer_slot;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_TIMER_DISARM, &m, NULL);
    SAFE_PRINT("[timer-demo] disarm before it fires -> rc=%ld\n", rc);
    ipc_sleep(40);
    m = (msg_regs_t){0};
    m.word[0] = notif_slot;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_POLL, &m, NULL);
    SAFE_PRINT("[timer-demo] poll after disarmed deadline passed -> rc=%ld (expect %d=wouldblock, never fired)\n",
            rc, IPC_ERR_WOULDBLOCK);
    m = (msg_regs_t){0};
    m.word[0] = timer_demo_untyped_slot;
    m.word[1] = 0x1;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_SIGNAL, &m, NULL);
    SAFE_PRINT("[timer-demo] signal against wrong-kind Untyped slot -> rc=%ld (expect %d=denied)\n",
            rc, IPC_ERR_NO_CAP);
    m = (msg_regs_t){0};
    m.word[0] = 9999;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_WAIT, &m, NULL);
    SAFE_PRINT("[timer-demo] wait against bogus slot -> rc=%ld (expect %d=denied)\n", rc, IPC_ERR_NO_CAP);
    m = (msg_regs_t){0};
    m.word[0] = timer_slot;
    m.word[1] = 9999;
    m.word[2] = 5;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_TIMER_ARM, &m, NULL);
    SAFE_PRINT("[timer-demo] arm against bogus notification slot -> rc=%ld (expect %d=denied)\n",
            rc, IPC_ERR_NO_CAP);
    tcb_t *helper = thread_create("timer-demo-helper", timer_demo_helper_entry,
                                  stack_timer_demo_helper + STACK_SIZE, 8);
    sched_lock_acquire();
    timer_demo_helper_notif_slot = kcap_next_slot();
    kcap_grant(helper->tid, CAP_KIND_NOTIFICATION, notif_idx, 0);
    sched_lock_release();
    ipc_sleep(5);
    sched_lock_acquire();
    int destroy_rc = sched_terminate(helper->tid);
    sched_lock_release();
    SAFE_PRINT("[timer-demo] destroyed helper (rc=%d) while it was WAIT_NOTIFICATION-blocked\n",
            destroy_rc);
    m = (msg_regs_t){0};
    m.word[0] = notif_slot;
    m.word[1] = 0xFEED;
    rc = robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_SIGNAL, &m, NULL);
    SAFE_PRINT("[timer-demo] signal after helper death -> rc=%ld (expect 0, no crash/hang)\n", rc);
    SAFE_PRINT("[timer-demo] all scenarios complete\n");
    for (;;) {
        ipc_sleep(SCHED_HZ * 10);
    }
}
static uint8_t stack_revoke_owner[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_revoke_toucher[STACK_SIZE] __attribute__((aligned(16)));
static uint64_t revoke_demo_frame_slot;
#define REVOKE_DEMO_SHARED_VA 0x71000000ULL
static void revoke_demo_toucher_entry(void) {
    volatile uint32_t *counter = (volatile uint32_t *)REVOKE_DEMO_SHARED_VA;
    uint32_t last = 0;
    uint64_t touches = 0;
    int reported = 0;
    for (;;) {
        uint32_t cur = ++(*counter);
        touches++;
        if (!reported && cur < last) {
            SAFE_PRINT("[revoke-demo] toucher observed non-monotonic counter after "
                    "revoke (last=%u now=%u touches=%lu) -- mapping was genuinely "
                    "revoked, not stale\n", last, cur, touches);
            reported = 1;
        }
        last = cur;
    }
}
static void revoke_demo_owner_entry(void) {
    SAFE_PRINT("[revoke-demo] owner: frame slot=%lu mapped at 0x%llx, toucher running\n",
            revoke_demo_frame_slot, (unsigned long long)REVOKE_DEMO_SHARED_VA);
    ipc_sleep(200);
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = revoke_demo_frame_slot;
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_REVOKE_FRAME, &m, NULL);
    SAFE_PRINT("[revoke-demo] owner: REVOKE_FRAME -> rc=%ld (expect 0)\n", rc);
    for (;;) {
        ipc_sleep(SCHED_HZ * 10);
    }
}
static uint8_t stack_notif_lat_waiter[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_notif_lat_signaler[STACK_SIZE] __attribute__((aligned(16)));
static uint64_t notif_lat_signaler_notif_slot;
static uint64_t notif_lat_waiter_notif_slot;
static volatile uint64_t notif_lat_signal_tsc;
#define NOTIF_LAT_ROUNDS 10
static void notif_lat_waiter_entry(void) {
    for (int round = 0; round < NOTIF_LAT_ROUNDS; round++) {
        msg_regs_t m = (msg_regs_t){0};
        m.word[0] = notif_lat_waiter_notif_slot;
        robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_WAIT, &m, NULL);
        uint64_t elapsed = rdtsc() - notif_lat_signal_tsc;
        msg_regs_t r = (msg_regs_t){0};
        r.word[0] = TEST_REPORT_KIND_NOTIF_LATENCY;
        r.word[1] = elapsed;
        r.word[2] = (uint64_t)round;
        ipc_send((tid_t)kinfo_user()->test_report_tid, &r);
    }
    for (;;) { ipc_sleep(1000000); }
}
static void notif_lat_signaler_entry(void) {
    for (int round = 0; round < NOTIF_LAT_ROUNDS; round++) {
        ipc_sleep(SCHED_HZ / 2);
        notif_lat_signal_tsc = rdtsc();
        msg_regs_t m = (msg_regs_t){0};
        m.word[0] = notif_lat_signaler_notif_slot;
        m.word[1] = 0x1;
        robu_ipc_raw(0, 0, IPC_FLAG_NOTIFY_SIGNAL, &m, NULL);
    }
    for (;;) { ipc_sleep(1000000); }
}
static uint8_t stack_app_supervisor[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_app_chaos_client[STACK_SIZE] __attribute__((aligned(16)));
static volatile tid_t hello_service_tid;
static volatile uint32_t hello_service_generation;
static tid_t spawn_hello_service(void) {
    tcb_t *t = elf_load_and_spawn("hello-service", service_elf_start, service_elf_end,
                                  8, PAGER_TID);
    hello_service_generation++;
    SAFE_PRINT("[app-supervisor] hello-service generation %u is tid=%u "
               "(elf parses=%lu cache_hits=%lu)\n",
               hello_service_generation, t->tid,
               elf_cache_stats.parses, elf_cache_stats.cache_hits);
    return t->tid;
}
static void app_supervisor_entry(void) {
    hello_service_tid = spawn_hello_service();
    for (;;) {
        ipc_sleep(SCHED_HZ / 2);
        if (!sched_get_tcb(hello_service_tid)) {
            SAFE_PRINT("[app-supervisor] tid=%u is gone -- restarting the service\n",
                       hello_service_tid);
            hello_service_tid = spawn_hello_service();
        }
    }
}
static void app_chaos_client_entry(void) {
    msg_regs_t m;
    tid_t from;
    uint64_t n = 0;
    uint64_t crashes_seen = 0;
    for (;;) {
        n++;
        m.word[0] = (n % 15 == 0) ? 0xDEAD : n;
        int64_t rc = ipc_call(hello_service_tid, &m, &from);
        if (rc == IPC_ERR_NOT_FOUND) {
            crashes_seen++;
            SAFE_PRINT("[app-chaos-client] request #%lu to tid=%u -> IPC_ERR_NOT_FOUND "
                       "(the service just crashed; will retry against whatever "
                       "the supervisor respawns) [%lu total]\n",
                       n, hello_service_tid, crashes_seen);
        } else if (n % 15 != 0 && (rc != IPC_ERR_NONE || m.word[0] != n + 1)) {
            SAFE_PRINT("[app-chaos-client] FAIL request #%lu rc=%ld got=%lu\n",
                       n, rc, m.word[0]);
        }
        ipc_sleep(SCHED_HZ / 5);
    }
}
void kmain(void) {
    arch_console_init();
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
    tcb_t *test_report = thread_create("test-report", test_report_entry,
                                       stack_test_report + STACK_SIZE, 14);
    kinfo_set_test_report_tid(test_report->tid);
    tcb_t *server = thread_create("echo-server", server_entry,
                                  stack_server + STACK_SIZE, 12);
    server_tid = server->tid;
    thread_create("client-A", client_a_entry, stack_client_a + STACK_SIZE, 10);
    thread_create("client-B", client_b_entry, stack_client_b + STACK_SIZE, 10);
    thread_create("spin-A", spinner_a_entry, stack_spin_a + STACK_SIZE, 5);
    thread_create("spin-B", spinner_b_entry, stack_spin_b + STACK_SIZE, 5);
    thread_create("stats", stats_entry, stack_stats + STACK_SIZE, 20);
    thread_create("console-in", console_in_entry, stack_console_in + STACK_SIZE, 20);
    spawn_ring3_task("ring3-A", 5);
    spawn_ring3_task("ring3-B", 5);
    tcb_t *driver = thread_create("mmio-driver", mmio_driver_entry,
                                  stack_mmio_driver + STACK_SIZE, 11);
    mmio_driver_tid = driver->tid;
    vm_cap_grant(mmio_driver_tid, VGA_MMIO_PADDR, 1, CAP_PERM_MAP);
    tcb_t *intruder = thread_create("mmio-intruder", mmio_intruder_entry,
                                    stack_mmio_intruder + STACK_SIZE, 11);
    mmio_intruder_tid = intruder->tid;
    thread_create("mmio-client", mmio_client_entry, stack_mmio_client + STACK_SIZE, 11);
    tcb_t *consumer = thread_create("xfer-consumer", xfer_consumer_entry,
                                    stack_xfer_consumer + STACK_SIZE, 9);
    consumer->address_space = vm_address_space_create();
    consumer->pager_tid = PAGER_TID;
    xfer_consumer_tid = consumer->tid;
    tcb_t *producer = thread_create("xfer-producer", xfer_producer_entry,
                                    stack_xfer_producer + STACK_SIZE, 9);
    producer->address_space = vm_address_space_create();
    producer->pager_tid = PAGER_TID;
    thread_create("supervisor", supervisor_entry, stack_supervisor + STACK_SIZE, 13);
    thread_create("chaos-client", chaos_client_entry, stack_chaos_client + STACK_SIZE, 6);
    tcb_t *shm_reader = thread_create("shm-reader", shm_reader_entry,
                                      stack_shm_reader + STACK_SIZE, 9);
    shm_reader->address_space = vm_address_space_create();
    shm_reader_tid = shm_reader->tid;
    tcb_t *shm_writer = thread_create("shm-writer", shm_writer_entry,
                                      stack_shm_writer + STACK_SIZE, 9);
    shm_writer->address_space = vm_address_space_create();
    tcb_t *batch_server = thread_create("batch-server", batch_server_entry,
                                        stack_batch_server + STACK_SIZE, 10);
    batch_server->address_space = vm_address_space_create();
    batch_server_tid = batch_server->tid;
    tcb_t *batch_client = thread_create("batch-client", batch_client_entry,
                                        stack_batch_client + STACK_SIZE, 9);
    batch_client->address_space = vm_address_space_create();
    tcb_t *share_bench_target = thread_create("share-bench-target", share_bench_target_entry,
                                              stack_share_bench_target + STACK_SIZE, 10);
    share_bench_target->address_space = vm_address_space_create();
    share_bench_target_tid = share_bench_target->tid;
    tcb_t *async_server = thread_create("async-server", async_server_entry,
                                        stack_async_server + STACK_SIZE, 12);
    async_server->address_space = vm_address_space_create();
    async_server_tid = async_server->tid;
    thread_create("async-client-0", async_client_0_entry, stack_async_client[0] + STACK_SIZE, 9);
    thread_create("async-client-1", async_client_1_entry, stack_async_client[1] + STACK_SIZE, 9);
    thread_create("async-client-2", async_client_2_entry, stack_async_client[2] + STACK_SIZE, 9);
    thread_create("sfi-domain-a", sfi_domain_a_entry, stack_sfi_domain_a + STACK_SIZE, 9);
    thread_create("sfi-domain-b", sfi_domain_b_entry, stack_sfi_domain_b + STACK_SIZE, 9);
    thread_create("app-supervisor", app_supervisor_entry,
                  stack_app_supervisor + STACK_SIZE, 13);
    thread_create("app-chaos-client", app_chaos_client_entry,
                  stack_app_chaos_client + STACK_SIZE, 6);
    elf_load_and_spawn("hello-task", task_elf_start, task_elf_end, 5, PAGER_TID);
    root_task_init(untyped_base, UNTYPED_REGION_SIZE);
    devfs_init();
    ramfs_init();
    procfs_init();
    sysfs_init();
    thread_create("devfs-demo", devfs_demo_entry, stack_devfs_demo + STACK_SIZE, 9);
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
    tcb_t *timer_demo = thread_create("timer-demo", timer_demo_entry,
                                      stack_timer_demo + STACK_SIZE, 9);
    timer_demo_untyped_slot = kcap_next_slot();
    kcap_grant(timer_demo->tid, CAP_KIND_UNTYPED, (uint64_t)untyped_base, UNTYPED_REGION_SIZE);
    uint64_t rt_slot, rt_addr;
    cap_retype(timer_demo->tid, (uint32_t)timer_demo_untyped_slot, CAP_KIND_NOTIFICATION,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    timer_demo_notif_slot = rt_slot;
    timer_demo_notif_idx = rt_addr;
    cap_retype(timer_demo->tid, (uint32_t)timer_demo_untyped_slot, CAP_KIND_TIMER,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    timer_demo_timer_slot = rt_slot;
    tcb_t *revoke_owner = thread_create("revoke-owner", revoke_demo_owner_entry,
                                        stack_revoke_owner + STACK_SIZE, 9);
    paddr_t revoke_demo_aspace = vm_address_space_create();
    revoke_owner->address_space = revoke_demo_aspace;
    revoke_owner->pager_tid = PAGER_TID;
    tcb_t *revoke_toucher = thread_create("revoke-toucher", revoke_demo_toucher_entry,
                                          stack_revoke_toucher + STACK_SIZE, 9);
    revoke_toucher->address_space = revoke_demo_aspace;
    revoke_toucher->pager_tid = PAGER_TID;
    uint64_t revoke_untyped_slot = kcap_next_slot();
    kcap_grant(revoke_owner->tid, CAP_KIND_UNTYPED, (uint64_t)untyped_base, UNTYPED_REGION_SIZE);
    cap_retype(revoke_owner->tid, (uint32_t)revoke_untyped_slot, CAP_KIND_FRAME,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    revoke_demo_frame_slot = rt_slot;
    tcb_t *notif_lat_signaler = thread_create("notif-lat-signaler", notif_lat_signaler_entry,
                                              stack_notif_lat_signaler + STACK_SIZE, 9);
    tcb_t *notif_lat_waiter = thread_create("notif-lat-waiter", notif_lat_waiter_entry,
                                            stack_notif_lat_waiter + STACK_SIZE, 9);
    uint64_t notif_lat_untyped_slot = kcap_next_slot();
    kcap_grant(notif_lat_signaler->tid, CAP_KIND_UNTYPED, (uint64_t)untyped_base, UNTYPED_REGION_SIZE);
    cap_retype(notif_lat_signaler->tid, (uint32_t)notif_lat_untyped_slot, CAP_KIND_NOTIFICATION,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    notif_lat_signaler_notif_slot = rt_slot;
    notif_lat_waiter_notif_slot = kcap_next_slot();
    kcap_grant(notif_lat_waiter->tid, CAP_KIND_NOTIFICATION, rt_addr, 0);
    scheduler_ready = 1;
    sched_start();
}
