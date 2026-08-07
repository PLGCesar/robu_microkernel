/*
 * Stage 3B "BENCHCLIENT" zone: the IPC fast-path benchmark's client
 * half (benchclient.c). See benchserver.ld's own comment for the base-
 * address and section-alignment reasoning, applied identically here.
 * 0x76000000: between benchserver.ld (0x74000000) and devfs.ld
 * (0x78000000), clear of both.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x0000000076000000;

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
