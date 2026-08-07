/*
 * Stage 3B "BENCHSERVER" zone: the IPC fast-path benchmark's server
 * half (benchserver.c). A distinct base address from every other zone,
 * same "an app's category is visible from its load address alone"
 * reasoning service.ld/task.ld/root.ld/devfs.ld already establish.
 * 0x74000000: verified clear of root.ld (0x70000000, plus its
 * ROOT_OWN_FRAME_VA sub-range at 0x71000000) and devfs.ld (0x78000000).
 *
 * Page-aligned between every section (not service.ld/task.ld's plain
 * ALIGN(8)) -- devfs.ld's own fix for a real bug found booting that
 * exact binary: a section with only zero-initialized globals lands
 * entirely in .bss, leaving an earlier ALIGN() attribute on an empty
 * section silently skipped by the linker. `. = ALIGN(4096);` between
 * every section forces the location counter forward unconditionally,
 * independent of whether the section that follows ends up empty --
 * applied here defensively even though this app's globals are simple.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x0000000074000000;

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
