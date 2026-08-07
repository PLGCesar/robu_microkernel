/*
 * Stage 3B "ABITEST" zone: the ABI conformance harness (abitest.c). See
 * benchserver.ld's own comment for the base-address and section-
 * alignment reasoning, applied identically here. 0x7a000000: clear of
 * every other zone (0x50000000/0x60000000/0x70000000/0x74000000/
 * 0x76000000/0x78000000), and of abitest.c's own UNMAPPED_VA constant
 * (0x77000000, deliberately unmapped, used for its own XFER/MAP/SHARE
 * negative tests).
 */
ENTRY(_start)

SECTIONS
{
    . = 0x000000007a000000;

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
