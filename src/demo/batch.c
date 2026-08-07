#include "robu/types.h"
#include "robu/sched.h"
#include "robu/pmm.h"
#include "robu/vm.h"
#include "robu/uipc.h"
#include "robu/kinfo.h"
#include "robu/testreport.h"
#include "../boot.h"

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

void batch_demo_init(void) {
    tcb_t *server = thread_create("batch-server", batch_server_entry,
                                  stack_batch_server + STACK_SIZE, 10);
    server->address_space = vm_address_space_create();
    batch_server_tid = server->tid;
    tcb_t *client = thread_create("batch-client", batch_client_entry,
                                  stack_batch_client + STACK_SIZE, 9);
    client->address_space = vm_address_space_create();
    tcb_t *target = thread_create("share-bench-target", share_bench_target_entry,
                                  stack_share_bench_target + STACK_SIZE, 10);
    target->address_space = vm_address_space_create();
    share_bench_target_tid = target->tid;
}
