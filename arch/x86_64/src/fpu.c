#include "robu/types.h"
#include "robu/arch.h"
#include "context.h"

/* Captured once at boot (after CR0/CR4 already enable SSE, see boot.S /
 * ap_trampoline.S) via fninit + a real reset MXCSR, then copied into every
 * new thread's tcb_t::fpu_state. An all-zero buffer is NOT a valid reset
 * image: MXCSR would come up with every exception mask cleared, so a
 * routine inexact-result on a brand-new thread's first float op would
 * immediately raise #XF. */
static uint8_t fpu_default_state[512] __attribute__((aligned(16)));

void arch_fpu_save(void *dst) {
    asm volatile("fxsave64 (%0)" : : "r"(dst) : "memory");
}

void arch_fpu_restore(const void *src) {
    asm volatile("fxrstor64 (%0)" : : "r"(src) : "memory");
}

void arch_fpu_boot_init(void) {
    asm volatile("fninit");
    uint32_t mxcsr_default = 0x1F80; /* all exceptions masked, round-to-nearest */
    asm volatile("ldmxcsr (%0)" : : "r"(&mxcsr_default) : "memory");
    arch_fpu_save(fpu_default_state);
}

void arch_fpu_default_state(void *dst) {
    memcpy(dst, fpu_default_state, sizeof(fpu_default_state));
}
