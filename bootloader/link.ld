/* linker.ld - link kernel at 0x1000 (matches loader) */
OUTPUT_FORMAT("elf32-i386")
ENTRY(_start)

SECTIONS
{
  . = 0x1000;

  .text :
  {
    *(.jumpstart)
    *(.text)
    *(.rodata)
  }

  .data :
  {
    *(.data)
  }

  .bss (NOLOAD) :
  {
    __bss_start = .;
    *(.bss)
    *(COMMON)
    . = ALIGN(4);
    __bss_end = .;
  }

  /DISCARD/ :
  {
    *(.eh_frame)
    *(.comment)
  }
}

