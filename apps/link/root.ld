/*
 * Stage 0C "ROOT" zone: the root task. A distinct base address from
 * both TASK (task.ld, 0x60000000) and SERVICE (service.ld, 0x50000000)
 * zones -- same "address space colonization is visible from the load
 * address alone" reasoning those two already establish. Comfortably
 * clear of ROOT_OWN_FRAME_VA (0x71000000, captable.c/root_task.c),
 * where a retyped Frame lands in this task's own address space.
 *
 * .rodata is page-aligned, unlike service.ld/task.ld's plain ALIGN(8)
 * -- a real bug, found by booting this exact binary: this is the
 * first app in the tree with genuine .rodata content (child_payload.S,
 * plus root_task.c's own extern-symbol references), so it's the first
 * to actually trigger ld.lld splitting .text (r-x) and .rodata (r--)
 * into two PT_LOAD segments. elf_load_and_spawn() (src/core/elf.c)
 * maps each segment's pages independently by page-rounding
 * [p_vaddr, p_vaddr+memsz) -- with only 8-byte alignment, .rodata's
 * segment rounded DOWN onto the SAME page .text's own segment had
 * already mapped, and the loader's per-segment loop mapped that page
 * a second time with .rodata's r-- (no-exec) permissions, clobbering
 * the executable code with a fresh zeroed frame. The result was a
 * genuine infinite fault loop at the entry address (thousands of
 * faults/second, a fresh physical frame leaked on every one) --
 * elf_load_and_spawn() eagerly maps every segment at spawn time, so
 * this should never have reached the fault path at all. service.ld/
 * task.ld have the identical latent gap; left alone here since
 * neither app has real .rodata content to trigger it today.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x0000000070000000;

    .text : {
        *(.text .text.*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(4096) {
        *(.data .data.*)
    }

    .bss : ALIGN(16) {
        *(COMMON)
        *(.bss .bss.*)
    }
}
