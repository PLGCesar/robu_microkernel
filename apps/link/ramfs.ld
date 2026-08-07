/*
 * Toybox-port foundation "RAMFS" zone: milestone 5's in-memory
 * filesystem server (apps/ramfs/ramfs.c). See apps/link/benchserver.ld's
 * own comment for the base-address and section-alignment reasoning,
 * applied identically here. 0x72000000: clear of every other zone
 * (0x50000000/0x60000000/0x70000000/0x74000000/0x76000000/0x78000000/
 * 0x7a000000/0x7c000000/0x7e000000/0x7e800000/0x7f000000).
 */
ENTRY(_start)

SECTIONS
{
    . = 0x0000000072000000;

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
