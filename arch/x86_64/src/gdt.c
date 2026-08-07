#include "robu/types.h"
#include "robu/arch.h"
#include "gdt.h"
typedef struct __attribute__((packed)) {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1, ist2, ist3, ist4, ist5, ist6, ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} tss64_t;
struct __attribute__((packed)) gdtr {
    uint16_t limit;
    uint64_t base;
};
extern uint8_t kstack_top[];
extern uint8_t ap_kstack_top[];
static uint64_t gdt[9] __attribute__((aligned(16)));
static tss64_t tss __attribute__((aligned(16)));
static tss64_t ap_tss __attribute__((aligned(16)));
struct gdtr kernel_gdt_ptr;
static uint64_t flat_entry(uint8_t access, uint8_t flags_nib) {
    uint64_t d = 0;
    d |= (uint64_t)access << 40;
    d |= (uint64_t)(flags_nib & 0xF) << 52;
    return d;
}
static void set_tss_entry(int idx, uint64_t base, uint32_t limit) {
    uint64_t low = 0;
    low |= (uint64_t)(limit & 0xFFFF);
    low |= (base & 0xFFFFFFULL) << 16;
    low |= (uint64_t)0x89 << 40;
    low |= (uint64_t)((limit >> 16) & 0xF) << 48;
    low |= ((base >> 24) & 0xFFULL) << 56;
    uint64_t high = (base >> 32) & 0xFFFFFFFFULL;
    gdt[idx] = low;
    gdt[idx + 1] = high;
}
void arch_gdt_init(void) {
    gdt[0] = 0;
    gdt[1] = flat_entry(0x9A, 0x2);
    gdt[2] = flat_entry(0x92, 0x0);
    gdt[3] = flat_entry(0xFA, 0x2);
    gdt[4] = flat_entry(0xF2, 0x0);
    for (int i = 0; i < (int)(sizeof(tss) / sizeof(uint64_t)); i++) {
        ((uint64_t *)&tss)[i] = 0;
    }
    tss.rsp0 = (uint64_t)kstack_top;
    tss.iomap_base = sizeof(tss);
    set_tss_entry(5, (uint64_t)&tss, sizeof(tss) - 1);
    kernel_gdt_ptr.limit = sizeof(gdt) - 1;
    kernel_gdt_ptr.base = (uint64_t)gdt;
    asm volatile(
        "lgdt %0\n"
        "mov %1, %%ds\n"
        "mov %1, %%es\n"
        "mov %1, %%ss\n"
        "mov %1, %%fs\n"
        "mov %1, %%gs\n"
        "pushq %2\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        : "m"(kernel_gdt_ptr), "r"((uint64_t)GDT_SEL_KDATA), "i"(GDT_SEL_KCODE)
        : "rax", "memory");
    asm volatile("ltr %0" : : "r"((uint16_t)GDT_SEL_TSS));
}
void arch_gdt_init_ap(void) {
    for (int i = 0; i < (int)(sizeof(ap_tss) / sizeof(uint64_t)); i++) {
        ((uint64_t *)&ap_tss)[i] = 0;
    }
    ap_tss.rsp0 = (uint64_t)ap_kstack_top;
    ap_tss.iomap_base = sizeof(ap_tss);
    set_tss_entry(7, (uint64_t)&ap_tss, sizeof(ap_tss) - 1);
    asm volatile("ltr %0" : : "r"((uint16_t)GDT_SEL_TSS_AP));
}
