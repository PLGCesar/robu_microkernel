ENTRY(_start)

PHDRS
{
    text PT_LOAD FILEHDR PHDRS;
    rodata PT_LOAD;
    data PT_LOAD;
    tls PT_TLS;
}

SECTIONS
{
    . = 0x000000007e000000;

    .text : {
        *(.text .text.*)
        *(.iplt)
    } :text

    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
        *(.gnu.warning.*)
    } :rodata

    .rela.dyn : {
        *(.rela.dyn)
    } :rodata

    . = ALIGN(4096);
    .data.rel.ro : {
        PROVIDE_HIDDEN(__init_array_start = .);
        *(.init_array*)
        PROVIDE_HIDDEN(__init_array_end = .);
        PROVIDE_HIDDEN(__fini_array_start = .);
        *(.fini_array*)
        PROVIDE_HIDDEN(__fini_array_end = .);
        *(.got)
        *(.got.plt)
    } :data

    .tdata : {
        *(.tdata .tdata.*)
    } :data :tls
    .tbss : {
        *(.tbss .tbss.*)
    } :data :tls
    /* .tbss is SHT_NOBITS (a TLS template, not a real data range following
     * it in the same PT_LOAD), so the location counter must be advanced by
     * hand -- otherwise the next real section silently aliases .tbss's
     * virtual address instead of getting its own, corrupting whatever
     * follows (this is exactly what happened here: it took stdin_box/
     * stdout_box down with it). */
    . = . + SIZEOF(.tbss);

    .data : {
        *(.data .data.*)
    } :data

    . = ALIGN(4096);
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data
}
