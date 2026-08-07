#include "robu/types.h"
#include "robu/sched.h"
#include "robu/uipc.h"
#include "robu/captable.h"
#include "../boot.h"

#define UNTYPED_REGION_SIZE (128u * 1024)

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

void timer_demo_init(paddr_t untyped_base) {
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
}
