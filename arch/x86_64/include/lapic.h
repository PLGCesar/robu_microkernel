#ifndef ARCH_X86_64_LAPIC_H
#define ARCH_X86_64_LAPIC_H
#include "robu/types.h"
#define LAPIC_PADDR 0xFEE00000ULL
#define LAPIC_REG_LVT_TIMER      0x320
#define LAPIC_REG_TIMER_INIT_COUNT 0x380
#define LAPIC_REG_TIMER_CUR_COUNT  0x390
#define LAPIC_REG_TIMER_DCR       0x3E0
#define LAPIC_LVT_MASKED   (1u << 16)
#define LAPIC_LVT_TIMER_ONESHOT (0u << 17)
#define LAPIC_TIMER_DIV_2   0x0
#define LAPIC_TIMER_DIV_4   0x1
#define LAPIC_TIMER_DIV_8   0x2
#define LAPIC_TIMER_DIV_16  0x3
#define LAPIC_TIMER_DIV_32  0x8
#define LAPIC_TIMER_DIV_64  0x9
#define LAPIC_TIMER_DIV_128 0xA
#define LAPIC_TIMER_DIV_1   0xB
void lapic_init(void);
uint32_t lapic_id(void);
void lapic_eoi(void);
void lapic_send_init_ipi(uint32_t target_apic_id);
void lapic_send_startup_ipi(uint32_t target_apic_id, uint8_t vector_page);
void lapic_send_ipi(uint32_t target_apic_id, uint8_t vector);
#endif
