/*
 * AP trampoline: real mode -> 32-bit protected mode -> 64-bit long
 * mode, landing in ap_entry() (C, arch/x86_64/src/smp.c). Copied byte-
 * for-byte to a fixed physical address (AP_TRAMPOLINE_PADDR) before
 * any SIPI is sent -- the same "raw bytes, position-independent,
 * start/end symbols" technique already used for the ring-3 payloads
 * (stage 2A/2B), just targeting a fixed low physical address instead
 * of a per-process virtual one.
 *
 * The AP starts here in real mode with CS = AP_TRAMPOLINE_VECTOR<<8,
 * IP=0 -- the SIPI hardware contract puts CS:IP at exactly this blob's
 * own physical location. Every label reference in the 16-bit portion
 * is therefore expressed as an offset from ap_trampoline_start (label
 * arithmetic the assembler resolves at assemble time), never as an
 * absolute address: the linker assigns this whole file addresses
 * around the kernel's own 1 MiB+ load address, which has nothing to do
 * with where these bytes actually end up living once copied.
 *
 * lgdt and the far jump both need a 32-bit (not the legacy 16-bit)
 * operand size even while still executing 16-bit code -- forced with
 * an explicit 0x66 prefix on lgdt (`ljmpl` already requests it for the
 * jump). Getting this wrong would silently truncate the addresses
 * being loaded; verified by disassembly, not just by the boot working.
 *
 * Once in 32-bit protected mode the flat GDT below covers all 4 GiB,
 * so ordinary absolute addressing works again -- including reaching
 * kernel symbols like boot_pml4 and kernel_gdt_ptr directly, since
 * this trampoline was copied next to a kernel that is still identity-
 * mapped and whose real, permanent GDT already exists by this point
 * (the AP is only ever woken after the BSP finishes its own init).
 */

#include "smp.h"

.section .rodata
.global ap_trampoline_start, ap_trampoline_end
.align 16
.code16
ap_trampoline_start:
    cli
    cld
    mov %cs, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    mov $(ap16_gdt_ptr - ap_trampoline_start), %bx
    .byte 0x66
    lgdt (%bx)

    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0          /* PE=1: protected mode, still fetching 16-bit code */

    ljmpl $0x08, $(AP_TRAMPOLINE_PADDR + (ap32_start - ap_trampoline_start))

.align 8
ap16_gdt:
    .quad 0
    .quad 0x00CF9A000000FFFF   /* 32-bit flat code, base=0 limit=4G */
    .quad 0x00CF92000000FFFF   /* 32-bit flat data, base=0 limit=4G */
ap16_gdt_end:
ap16_gdt_ptr:
    .word ap16_gdt_end - ap16_gdt - 1
    .long AP_TRAMPOLINE_PADDR + (ap16_gdt - ap_trampoline_start)

.code32
ap32_start:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4           /* PAE */

    .extern boot_pml4
    mov $boot_pml4, %eax     /* same tables the BSP already fully built */
    mov %eax, %cr3

    mov $0xC0000080, %ecx
    rdmsr
    or $((1 << 8) | (1 << 11)), %eax
    wrmsr                    /* EFER.LME | EFER.NXE */

    mov %cr0, %eax
    or $0x80000000, %eax
    mov %eax, %cr0           /* paging on */

    .extern kernel_gdt_ptr
    lgdt kernel_gdt_ptr      /* the kernel's real GDT, not this trampoline's temp one */
    ljmp $0x08, $ap64_start

.code64
ap64_start:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss

    .extern ap_kstack_top
    movq $ap_kstack_top, %rsp
    xorq %rbp, %rbp

    .extern ap_entry
    call ap_entry
1:  cli
    hlt
    jmp 1b
ap_trampoline_end:

.section .note.GNU-stack, "", @progbits
