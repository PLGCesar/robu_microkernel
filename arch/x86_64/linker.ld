ENTRY(start)

/*
 * The PVH note must land in a PT_NOTE program header — that is what QEMU
 * scans to find the 32-bit entry point — so the segments are spelled out
 * explicitly instead of trusting orphan-section placement.
 */
PHDRS
{
    text   PT_LOAD  FLAGS(5); /* R+X */
    rodata PT_LOAD  FLAGS(4); /* R   */
    data   PT_LOAD  FLAGS(6); /* R+W */
    note   PT_NOTE  FLAGS(4);
}

SECTIONS
{
    /* Load low: QEMU places us at 1 MiB physical, identity-mapped at boot. */
    . = 1M;

    .text : {
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
