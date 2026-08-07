#include "robu/types.h"
#include "robu/sched.h"
#include "robu/uipc.h"
#include "../boot.h"

#define SANDBOX_SIZE 4096

static uint8_t sandbox_region[SANDBOX_SIZE] __attribute__((aligned(16)));
static uint8_t stack_sfi_domain_a[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t stack_sfi_domain_b[STACK_SIZE] __attribute__((aligned(16)));

static int sandbox_write(uint32_t offset, uint8_t value) {
    if (offset >= SANDBOX_SIZE) {
        return -1;
    }
    sandbox_region[offset] = value;
    return 0;
}

static int sandbox_read(uint32_t offset, uint8_t *out) {
    if (offset >= SANDBOX_SIZE) {
        return -1;
    }
    *out = sandbox_region[offset];
    return 0;
}

static void sfi_domain_a_entry(void) {
    for (uint32_t i = 0; i < SANDBOX_SIZE; i++) {
        sandbox_write(i, (uint8_t)(i & 0xFF));
    }
    SAFE_PRINT("[sfi] domain-A filled the shared sandbox through the bounds-"
               "checked gate (as=0x%lx)\n", current_thread->address_space);
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}

static void sfi_domain_b_entry(void) {
    ipc_sleep(SCHED_HZ / 4);
    uint8_t v = 0;
    int rc_ok = sandbox_read(10, &v);
    int rc_oob = sandbox_write(SANDBOX_SIZE + 100, 0xFF);
    SAFE_PRINT("[sfi] domain-B (as=0x%lx, same as domain-A -- no MMU boundary "
               "between them) in-bounds read [10]=%u (rc=%d); out-of-bounds "
               "write denied by the gate itself, no hardware fault involved "
               "(rc=%d)\n", current_thread->address_space, v, rc_ok, rc_oob);
    for (;;) { ipc_sleep(SCHED_HZ * 1000); }
}

void sfi_demo_init(void) {
    thread_create("sfi-domain-a", sfi_domain_a_entry, stack_sfi_domain_a + STACK_SIZE, 9);
    thread_create("sfi-domain-b", sfi_domain_b_entry, stack_sfi_domain_b + STACK_SIZE, 9);
}
