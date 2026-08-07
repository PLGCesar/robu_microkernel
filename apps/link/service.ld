/*
 * Stage 2D "SERVICE" zone: request/reply-style apps that talk to a
 * supervisor and can be respawned (hello_service.c). A distinct base
 * address from the TASK zone (task.ld) is what makes "address space
 * colonization" a real, inspectable property rather than a label --
 * an app's category is visible from its load address alone (e.g. in
 * the [app-supervisor] print lines: 0x50xxxxxx vs 0x60xxxxxx).
 */
ENTRY(_start)

SECTIONS
{
    . = 0x0000000050000000;

    .text : {
        *(.text .text.*)
    }

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(8) {
        *(.data .data.*)
    }

    .bss : ALIGN(16) {
        *(COMMON)
        *(.bss .bss.*)
    }
}
