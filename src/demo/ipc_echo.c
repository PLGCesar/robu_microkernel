#include "robu/types.h"
#include "robu/arch.h"
#include "robu/sched.h"
#include "robu/pmm.h"
#include "robu/uipc.h"
#include "robu/elf.h"
#include "percpu.h"
#include "../boot.h"

static uint8_t stack_server[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_client_a[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_client_b[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_spin_a[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_spin_b[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_stats[STACK_SIZE] __attribute__((aligned(16)));

static tid_t server_tid;
static volatile uint64_t spin_count_a;
static volatile uint64_t spin_count_b;
static volatile uint64_t client_rt_a;
static volatile uint64_t client_rt_b;

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

void ipc_echo_demo_init(void) {
    tcb_t *server = thread_create("echo-server", server_entry, stack_server + STACK_SIZE, 12);
    server_tid = server->tid;
    thread_create("client-A", client_a_entry, stack_client_a + STACK_SIZE, 10);
    thread_create("client-B", client_b_entry, stack_client_b + STACK_SIZE, 10);
    thread_create("spin-A", spinner_a_entry, stack_spin_a + STACK_SIZE, 5);
    thread_create("spin-B", spinner_b_entry, stack_spin_b + STACK_SIZE, 5);
    thread_create("stats", stats_entry, stack_stats + STACK_SIZE, 20);
}
