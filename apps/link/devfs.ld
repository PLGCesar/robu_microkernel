/*
 * Stage 0E "DEVFS" zone: the userspace /dev filesystem server
 * (devfs.c). A distinct base address from every other zone -- same
 * "an app's category is visible from its load address alone" reasoning
 * service.ld/task.ld/root.ld already establish. 0x78000000: verified
 * clear of service.ld (0x50000000), task.ld (0x60000000), root.ld
 * (0x70000000, plus its own small ROOT_OWN_FRAME_VA sub-range at
 * 0x71000000) -- still comfortably inside the private per-address-
 * space window every app zone shares ([0x40000000, 0x80000000), one
 * PDPT slot), well clear of 0x80000000 where the *shared* KINFO_VA/
 * LAPIC MMIO region begins.
 *
 * A real bug, found booting this exact binary: `.data : ALIGN(4096) {
 * ... }` (root.ld's own syntax, copied here) only forces alignment
 * when the output section actually receives input content. devfs.c's
 * one global (rdrand_available) is zero-initialized, so it lands in
 * .bss, not .data -- .data ends up genuinely empty, ld.lld skips its
 * ALIGN entirely, and the following .bss (still just ALIGN(16), same
 * as every other zone's script) landed mid-page, sharing .rodata's
 * frame -- the pager then "resolved" every fault at that shared page
 * by handing out a fresh, unrelated frame, an infinite fault loop
 * disguised as lazy allocation working normally. Fixed with explicit
 * `. = ALIGN(4096);` between every section instead of a per-section
 * attribute -- forces the location counter forward unconditionally,
 * independent of whether the section that follows ends up empty.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x0000000078000000;

    .text : {
        *(.text .text.*)
    }

    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
    }

    . = ALIGN(4096);
    .data : {
        *(.data .data.*)
    }

    . = ALIGN(4096);
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    }
}
