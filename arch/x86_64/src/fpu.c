#include "robu/types.h"
#include "robu/arch.h"
#include "context.h"

static uint8_t fpu_default_state[512] __attribute__((aligned(16)));

void arch_fpu_save(void *dst) {
    asm volatile("fxsave64 (%0)" : : "r"(dst) : "memory");
}

void arch_fpu_restore(const void *src) {
    asm volatile("fxrstor64 (%0)" : : "r"(src) : "memory");
}

void arch_fpu_boot_init(void) {
    asm volatile("fninit");
    uint32_t mxcsr_default = 0x1F80;
    asm volatile("ldmxcsr (%0)" : : "r"(&mxcsr_default) : "memory");
    arch_fpu_save(fpu_default_state);
}

void arch_fpu_default_state(void *dst) {
    memcpy(dst, fpu_default_state, sizeof(fpu_default_state));
}

#define MSR_FS_BASE 0xC0000100u

void arch_set_fsbase(uint64_t base) {
    uint32_t lo = (uint32_t)base;
    uint32_t hi = (uint32_t)(base >> 32);
    asm volatile("wrmsr" : : "c"(MSR_FS_BASE), "a"(lo), "d"(hi) : "memory");
}
