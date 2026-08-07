#include "robu/types.h"
#include "robu/arch.h"
#include "robu/kprintf.h"
#include "robu/sched.h"
#include "smp.h"
#include "lapic.h"
#include "percpu.h"
extern const uint8_t ap_trampoline_start[];
extern const uint8_t ap_trampoline_end[];
extern uint8_t ap_kstack_top[];
static volatile uint32_t ap_alive_flag;
volatile uint32_t scheduler_ready;
volatile uint32_t ap_joined_flag;
void ap_entry(void) {
    percpu_init_this_cpu(1, lapic_id(), ap_kstack_top);
    arch_gdt_init_ap();
    arch_intr_init_ap();
    lapic_init();
    arch_timer_percpu_init();
    ap_alive_flag = 0xCAFEBABE;
    kprintf("[smp] AP cpu_id=%u apic_id=%u has its own GS-base, TSS, IDT\n",
            this_cpu()->cpu_id, this_cpu()->apic_id);
    while (!scheduler_ready) {
        asm volatile("pause");
    }
    sched_init_ap();
    ap_joined_flag = 1;
    kprintf("[smp] AP cpu_id=%u joining the scheduler\n", this_cpu()->cpu_id);
    sched_join_ap();
}
void smp_start_ap(void) {
    uint64_t len = (uint64_t)(ap_trampoline_end - ap_trampoline_start);
    memcpy((void *)AP_TRAMPOLINE_PADDR, ap_trampoline_start, len);
    lapic_send_init_ipi(AP_APIC_ID);
    lapic_send_startup_ipi(AP_APIC_ID, AP_TRAMPOLINE_VECTOR);
    lapic_send_startup_ipi(AP_APIC_ID, AP_TRAMPOLINE_VECTOR);
    for (volatile uint32_t i = 0; i < 50000000; i++) {
        if (ap_alive_flag == 0xCAFEBABE) {
            kprintf("[smp] AP apic_id=%u online\n", AP_APIC_ID);
            return;
        }
    }
    kprintf("[smp] AP did not respond within the timeout\n");
}
