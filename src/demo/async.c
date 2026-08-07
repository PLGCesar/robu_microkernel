#include "robu/types.h"
#include "robu/sched.h"
#include "robu/vm.h"
#include "robu/uipc.h"
#include "../boot.h"

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

void async_demo_init(void) {
    tcb_t *server = thread_create("async-server", async_server_entry,
                                  stack_async_server + STACK_SIZE, 12);
    server->address_space = vm_address_space_create();
    async_server_tid = server->tid;
    thread_create("async-client-0", async_client_0_entry,
                  stack_async_client[0] + STACK_SIZE, 9);
    thread_create("async-client-1", async_client_1_entry,
                  stack_async_client[1] + STACK_SIZE, 9);
    thread_create("async-client-2", async_client_2_entry,
                  stack_async_client[2] + STACK_SIZE, 9);
}
