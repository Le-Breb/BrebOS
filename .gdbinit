# Max size that GDB can allocate for the contents of a value. Default is 36536, which is too small for process class
# set max-value-size 10000000

file isodir/boot/kernel.elf
set architecture i8086
layout asm
layout regs
target remote localhost:26000