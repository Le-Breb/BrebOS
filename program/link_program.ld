SEARCH_DIR("build")

PHDRS
{
    headers PT_PHDR PHDRS;
    interp PT_INTERP;
    text PT_LOAD FILEHDR PHDRS;
    data PT_LOAD;
    dynamic PT_DYNAMIC;
}

SECTIONS
{
    . = 0; /* relocate to address 0 */

    /* PHDR segment */
    . = SIZEOF_HEADERS;
    .interp ALIGN(4) :
    {
        *(.interp)
    } :interp :text

    .text ALIGN(4) :
    {
        start_program.o(.text) /* include the .text section of start.o */
        *(.text) /* include all other .text sections */
    } :text

    .rodata ALIGN(4) :
    {
        *(.rodata*)
    } :text

    .data ALIGN(4) :
    {
        *(.data)
    } :data

    .dynamic ALIGN(4) :
    {
        *(.dynamic)
    } :data

    .got ALIGN(4) :
    {
        *(.got)
    } :data

    .bss ALIGN(4) :
    {
        *(.bss)
    } :data

    .plt : { *(.plt) }
    .rel.plt : { *(.rel.plt) }
    .dynsym : { *(.dynsym) }
    .dynstr : { *(.dynstr) }
    .rel.dyn : { *(.rel.dyn) }
    .got.plt : { *(.got.plt) }

    /DISCARD/ : { *(.note.GNU-stack) }
}