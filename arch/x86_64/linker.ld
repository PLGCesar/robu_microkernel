ENTRY(start)

PHDRS
{
    text   PT_LOAD  FLAGS(5);
    rodata PT_LOAD  FLAGS(4);
    data   PT_LOAD  FLAGS(6);
    note   PT_NOTE  FLAGS(4);
}

SECTIONS
{
    . = 1M;

    .text : {
        KEEP(*(.multiboot2))
        *(.text .text.*)
    } :text

    .note.Xen : {
        KEEP(*(.note.Xen))
    } :text :note

    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
    } :rodata

    .data : ALIGN(8) {
        *(.data .data.*)
    } :data
    _edata = .;

    .bss : ALIGN(16) {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    _end = .;
}
