#include "robu/types.h"
#include "robu/uipc.h"
#include "robu/captable.h"
#include "robu/kinfo.h"
#include "robu/testreport.h"
#define ROOT_UNTYPED_SLOT 0
#define MONITOR_TID        2
#define ROOT_OWN_FRAME_VA 0x71000000ULL
#define CHILD_ENTRY_VA 0x40800000ULL
extern const uint8_t child_payload_start[], child_payload_end[];
extern const uint64_t child_payload_counter_offset;
static uint64_t retype(uint64_t untyped_slot, uint64_t kind, uint64_t as_slot,
                       uint64_t entry, uint64_t stack, uint64_t frame_slot,
                       uint64_t *out_addr) {
    msg_regs_t m;
    m.word[0] = untyped_slot;
    m.word[1] = kind;
    m.word[2] = as_slot;
    m.word[3] = entry;
    m.word[4] = stack;
    m.word[5] = frame_slot;
    int64_t rc = robu_ipc_raw(0, 0, IPC_FLAG_RETYPE, &m, NULL);
    if (rc != 0) {
        return (uint64_t)-1;
    }
    if (out_addr) {
        *out_addr = m.word[1];
    }
    return m.word[0];
}
static int64_t destroy(uint64_t slot) {
    msg_regs_t m;
    m.word[0] = slot;
    m.word[1] = 0;
    m.word[2] = 0;
    m.word[3] = 0;
    m.word[4] = 0;
    m.word[5] = 0;
    return robu_ipc_raw(0, 0, IPC_FLAG_DESTROY, &m, NULL);
}
void _start(void) {
    uint64_t frame_slot = retype(ROOT_UNTYPED_SLOT, CAP_KIND_FRAME, 0, 0, 0, 0, NULL);
    volatile uint8_t *own_view = (volatile uint8_t *)ROOT_OWN_FRAME_VA;
    uint64_t payload_len = (uint64_t)(child_payload_end - child_payload_start);
    for (uint64_t i = 0; i < payload_len; i++) {
        own_view[i] = child_payload_start[i];
    }
    uint64_t as_slot = retype(ROOT_UNTYPED_SLOT, CAP_KIND_ADDRSPACE, 0, 0, 0, 0, NULL);
    uint64_t tcb_slot = retype(ROOT_UNTYPED_SLOT, CAP_KIND_TCB, as_slot, CHILD_ENTRY_VA, 0,
                               frame_slot, NULL);
    uint64_t exhausted_after = 0;
    for (uint64_t i = 0; i < 40; i++) {
        uint64_t slot = retype(ROOT_UNTYPED_SLOT, CAP_KIND_FRAME, 0, 0, 0, 0, NULL);
        if (slot == (uint64_t)-1) {
            exhausted_after = i;
            break;
        }
    }
    const volatile uint32_t *counter =
        (const volatile uint32_t *)(ROOT_OWN_FRAME_VA + child_payload_counter_offset);
    uint32_t last = 0xFFFFFFFF;
    uint32_t changes_seen = 0;
    int64_t destroy_rc = 1;
    int64_t redestroy_rc = 1;
    for (;;) {
        uint32_t cur = *counter;
        if (cur != last) {
            last = cur;
            changes_seen++;
            msg_regs_t m;
            m.word[0] = cur;
            m.word[1] = exhausted_after;
            m.word[2] = (uint64_t)destroy_rc;
            m.word[3] = (uint64_t)redestroy_rc;
            ipc_send(MONITOR_TID, &m);
        }
        if (changes_seen >= 15 && destroy_rc == 1) {
            destroy_rc = destroy(tcb_slot);
            redestroy_rc = destroy(tcb_slot);
            const volatile kinfo_page_t *kinfo = (const volatile kinfo_page_t *)KINFO_VA;
            msg_regs_t report = (msg_regs_t){0};
            report.word[0] = TEST_REPORT_KIND_DESTROY;
            report.word[1] = (uint64_t)destroy_rc;
            report.word[2] = (uint64_t)redestroy_rc;
            ipc_send((tid_t)kinfo->test_report_tid, &report);
        }
        ipc_sleep(2);
    }
}
