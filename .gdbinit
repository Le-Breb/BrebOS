# Max size that GDB can allocate for the contents of a value. Default is 36536, which is too small for process class
# set max-value-size 10000000

file isodir/boot/kernel.elf
set architecture i8086
source gdb_elf_loader.py
#  layout asm
# layout reg
# target remote localhost:26000