#include "robu/types.h"
#include "robu/sched.h"
#include "robu/pmm.h"
#include "robu/vm.h"
#include "robu/uipc.h"
#include "../boot.h"

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

void xfer_demo_init(void) {
    tcb_t *consumer = thread_create("xfer-consumer", xfer_consumer_entry,
                                    stack_xfer_consumer + STACK_SIZE, 9);
    consumer->address_space = vm_address_space_create();
    consumer->pager_tid = PAGER_TID;
    xfer_consumer_tid = consumer->tid;

    tcb_t *producer = thread_create("xfer-producer", xfer_producer_entry,
                                    stack_xfer_producer + STACK_SIZE, 9);
    producer->address_space = vm_address_space_create();
    producer->pager_tid = PAGER_TID;
}
