/*
 * Stage 2D "TASK" zone: steady-state, fire-and-forget apps (hello_task.c).
 * See service.ld for why a distinct base address matters.
 */
ENTRY(_start)

SECTIONS
{
    . = 0x0000000060000000;

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
