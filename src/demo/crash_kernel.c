#include "robu/types.h"
#include "robu/arch.h"
#include "robu/sched.h"
#include "robu/pmm.h"
#include "robu/vm.h"
#include "robu/uipc.h"
#include "../boot.h"

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

void crash_kernel_demo_init(void) {
    thread_create("supervisor", supervisor_entry, stack_supervisor + STACK_SIZE, 13);
    thread_create("chaos-client", chaos_client_entry, stack_chaos_client + STACK_SIZE, 6);
}
