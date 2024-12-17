SEARCH_DIR("build")

PHDRS
{
    headers PT_PHDR PHDRS;
    interp PT_INTERP;
    text PT_LOAD FILEHDR PHDRS FLAGS(5);
    data PT_LOAD FLAGS(6);
    dynamic PT_DYNAMIC;
}

SECTIONS
{
    . = 0x1000 + SIZEOF_HEADERS; /* relocate to address 0x1000 to segfault on address 0 */

    .interp ALIGN(4) :
    {
        *(.interp)
    } :interp :text

    .text ALIGN(0x1000) :
    {
        start_shell.o(.text) /* include the .text section of start.o */
        *(.text) /* include all other .text sections */
    } :text

    .rodata ALIGN(4) :
    {
        *(.rodata*)
    } :text

    .data ALIGN(0x1000) :
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