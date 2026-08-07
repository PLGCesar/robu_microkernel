/*
 * sh-replacement plan "CONSOLETEST" zone: milestone 5's verification
 * app (apps/consoletest/consoletest.c), linked against the real
 * apps/libc shim like spawntest.ld. See apps/link/benchserver.ld's own
 * comment for the base-address and section-alignment reasoning,
 * applied identically here. 0x7b000000: clear of every other zone
 * (0x50000000/0x60000000/0x70000000/0x72000000-0x73900000/
 * 0x74000000/0x76000000/0x78000000/0x7a000000/0x7c000000/0x7d000000/
 * 0x7e000000/0x7e800000/0x7f000000).
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
