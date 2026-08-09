/*
 * The ring-3 page-fault pager (apps/pager/pager.c). See
 * apps/link/sysfs.ld's own comment for the base-address and
 * section-alignment reasoning, applied identically here. 0x7b000000: free
 * (diskfs.ld's own comment already reserved it, alongside 0x70000000/
 * 0x7d000000/0x7f000000).
 */
ENTRY(_start)

SECTIONS
{
    . = 0x000000007b000000;

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
