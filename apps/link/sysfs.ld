/*
 * /dev /proc /var/tmp /var/sys completion "SYSFS" zone: the /var/sys
 * server (apps/sysfs/sysfs.c). See apps/link/benchserver.ld's own
 * comment for the base-address and section-alignment reasoning,
 * applied identically here. 0x75000000: clear of every other zone --
 * the one free gap between benchserver's 0x74000000 and benchclient's
 * 0x76000000 (see apps/link/procfs.ld's own comment for the fuller
 * list this was checked against).
 */
ENTRY(_start)

SECTIONS
{
    . = 0x0000000075000000;

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
