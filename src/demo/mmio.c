#include "robu/types.h"
#include "robu/sched.h"
#include "robu/vm.h"
#include "robu/uipc.h"
#include "robu/captable.h"
#include "../boot.h"

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

void mmio_demo_init(void) {
    tcb_t *driver = thread_create("mmio-driver", mmio_driver_entry,
                                  stack_mmio_driver + STACK_SIZE, 11);
    mmio_driver_tid = driver->tid;
    vm_cap_grant(mmio_driver_tid, VGA_MMIO_PADDR, 1, CAP_PERM_MAP);
    tcb_t *intruder = thread_create("mmio-intruder", mmio_intruder_entry,
                                    stack_mmio_intruder + STACK_SIZE, 11);
    mmio_intruder_tid = intruder->tid;
    thread_create("mmio-client", mmio_client_entry, stack_mmio_client + STACK_SIZE, 11);
}
