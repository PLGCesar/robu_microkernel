#include "robu/types.h"
#include "robu/sched.h"
#include "robu/uipc.h"
#include "robu/elf.h"
#include "../boot.h"

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

void app_resilience_demo_init(void) {
    thread_create("app-supervisor", app_supervisor_entry,
                  stack_app_supervisor + STACK_SIZE, 13);
    thread_create("app-chaos-client", app_chaos_client_entry,
                  stack_app_chaos_client + STACK_SIZE, 6);
}
