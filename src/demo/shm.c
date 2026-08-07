#include "robu/types.h"
#include "robu/sched.h"
#include "robu/pmm.h"
#include "robu/vm.h"
#include "robu/uipc.h"
#include "../boot.h"

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

void shm_demo_init(void) {
    tcb_t *reader = thread_create("shm-reader", shm_reader_entry,
                                  stack_shm_reader + STACK_SIZE, 9);
    reader->address_space = vm_address_space_create();
    shm_reader_tid = reader->tid;
    tcb_t *writer = thread_create("shm-writer", shm_writer_entry,
                                  stack_shm_writer + STACK_SIZE, 9);
    writer->address_space = vm_address_space_create();
}
