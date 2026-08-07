#include "robu/types.h"
#include "robu/sched.h"
#include "robu/vm.h"
#include "robu/uipc.h"
#include "robu/captable.h"
#include "../boot.h"

#define UNTYPED_REGION_SIZE (128u * 1024)
#define REVOKE_DEMO_SHARED_VA 0x71000000ULL

static uint8_t stack_revoke_owner[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_revoke_toucher[STACK_SIZE] __attribute__((aligned(16)));
static uint64_t revoke_demo_frame_slot;

static void revoke_demo_toucher_entry(void) {
    volatile uint32_t *counter = (volatile uint32_t *)REVOKE_DEMO_SHARED_VA;
    uint32_t last = 0;
    uint64_t touches = 0;
    int reported = 0;
    for (;;) {
        uint32_t cur = ++(*counter);
        touches++;
        if (!reported && cur < last) {
            SAFE_PRINT("[revoke-demo] toucher observed non-monotonic counter after "
                    "revoke (last=%u now=%u touches=%lu) -- mapping was genuinely "
                    "revoked, not stale\n", last, cur, touches);
            reported = 1;
        }
        last = cur;
    }
}

static void revoke_demo_owner_entry(void) {
    SAFE_PRINT("[revoke-demo] owner: frame slot=%lu mapped at 0x%llx, toucher running\n",
            revoke_demo_frame_slot, (unsigned long long)REVOKE_DEMO_SHARED_VA);
    ipc_sleep(200);
    msg_regs_t m = (msg_regs_t){0};
    m.word[0] = revoke_demo_frame_slot;
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_REVOKE_FRAME, &m, NULL);
    SAFE_PRINT("[revoke-demo] owner: REVOKE_FRAME -> rc=%ld (expect 0)\n", rc);
    for (;;) {
        ipc_sleep(SCHED_HZ * 10);
    }
}

void revoke_demo_init(paddr_t untyped_base) {
    tcb_t *owner = thread_create("revoke-owner", revoke_demo_owner_entry,
                                 stack_revoke_owner + STACK_SIZE, 9);
    paddr_t aspace = vm_address_space_create();
    owner->address_space = aspace;
    owner->pager_tid = PAGER_TID;
    tcb_t *toucher = thread_create("revoke-toucher", revoke_demo_toucher_entry,
                                   stack_revoke_toucher + STACK_SIZE, 9);
    toucher->address_space = aspace;
    toucher->pager_tid = PAGER_TID;
    uint64_t untyped_slot = kcap_next_slot();
    kcap_grant(owner->tid, CAP_KIND_UNTYPED, (uint64_t)untyped_base, UNTYPED_REGION_SIZE);
    uint64_t rt_slot, rt_addr;
    cap_retype(owner->tid, (uint32_t)untyped_slot, CAP_KIND_FRAME,
              0, 0, 0, 0, &rt_slot, &rt_addr);
    revoke_demo_frame_slot = rt_slot;
}
