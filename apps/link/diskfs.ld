/*
 * The disk-backed /mnt/disk0 server (apps/diskfs/diskfs.c). See
 * apps/link/sysfs.ld's own comment for the base-address and
 * section-alignment reasoning, applied identically here. 0x76000000: free
 * (between minibox's 0x77000000 and... actually below it, in the gap
 * bootfs's own comment left free alongside 0x70000000/0x7b000000/
 * 0x7d000000/0x7f000000).
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
