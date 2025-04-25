ENTRY(_start)           /* the name of the entry symbol */

SECTIONS {
    . = 0xC0100000;          /* the code should be relocated to 3GB + 1MB */
    _kernel_start = .;       /* global symbol for the kernel start */
    _kernel_phys_start = . - 0xC0000000; /* physical address of the kernel start */

    /* align at 4 KB and load at 1 MB */
    .text ALIGN (0x1000) : AT(ADDR(.text)-0xC0000000)
    {
        *(.multiboot2) /* first the multiboot2 header */
        *(.text)             /* all text sections from all files */
    }

    /* align at 4 KB and load at 1 MB + . */
    .rodata ALIGN (0x1000) : AT(ADDR(.rodata)-0xC0000000)
    {
        *(.rodata*)          /* all read-only data sections from all files */
    }

    /* align at 4 KB and load at 1 MB + . */
    .data ALIGN (0x1000) : AT(ADDR(.data)-0xC0000000)
    {
        *(.data)             /* all data sections from all files */
    }

    /* align at 4 KB and load at 1 MB + . */
    .bss ALIGN (0x1000) : AT(ADDR(.bss)-0xC0000000)
    {
        *(COMMON)            /* all COMMON sections from all files */
        *(.bss)              /* all bss sections from all files */
    }

    _kernel_end = .;         /* global symbol for the kernel end */
    _kernel_phys_end = . - 0xC0000000; /* physical address of the kernel end */
}