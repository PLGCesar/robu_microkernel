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
        *(.init_array*)
        *(.fini_array*)
        *(.got)
        *(.got.plt)
    } :data

    .tdata : {
        *(.tdata .tdata.*)
    } :data :tls
    .tbss : {
        *(.tbss .tbss.*)
    } :data :tls

    .data : {
        *(.data .data.*)
    } :data

    . = ALIGN(4096);
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data
}
