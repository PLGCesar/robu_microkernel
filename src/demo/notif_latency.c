#include "robu/types.h"
#include "robu/sched.h"
#include "robu/uipc.h"
#include "robu/captable.h"
#include "robu/kinfo.h"
#include "robu/testreport.h"
#include "../boot.h"

#define UNTYPED_REGION_SIZE (128u * 1024)
#define NOTIF_LAT_ROUNDS 10

static uint8_t stack_notif_lat_waiter[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_notif_lat_signaler[STACK_SIZE] __attribute__((aligned(16)));
static uint64_t notif_lat_signaler_notif_slot;
static uint64_t notif_lat_waiter_notif_slot;
static volatile uint64_t notif_lat_signal_tsc;

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

void notif_latency_demo_init(paddr_t untyped_base) {
    tcb_t *signaler = thread_create("notif-lat-signaler", notif_lat_signaler_entry,
                                    stack_notif_lat_signaler + STACK_SIZE, 9);
    tcb_t *waiter = thread_create("notif-lat-waiter", notif_lat_waiter_entry,
                                  stack_notif_lat_waiter + STACK_SIZE, 9);
    uint64_t untyped_slot = kcap_next_slot();
    kcap_grant(signaler->tid, CAP_KIND_UNTYPED, (uint64_t)untyped_base, UNTYPED_REGION_SIZE);
    uint64_t rt_slot, rt_addr;
    cap_retype(signaler->tid, (uint32_t)untyped_slot, CAP_KIND_NOTIFICATION,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    notif_lat_signaler_notif_slot = rt_slot;
    notif_lat_waiter_notif_slot = kcap_next_slot();
    kcap_grant(waiter->tid, CAP_KIND_NOTIFICATION, rt_addr, 0);
}
