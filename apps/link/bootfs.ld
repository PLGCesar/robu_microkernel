/*
 * The /boot server (apps/bootfs/bootfs.c) -- a read-only view of the
 * kernel-resident rootfs_buf archive. See apps/link/sysfs.ld's own comment
 * for the base-address and section-alignment reasoning, applied identically
 * here. 0x74000000: free (between ramfstest's 0x73000000 and sysfs's
 * 0x75000000).
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
