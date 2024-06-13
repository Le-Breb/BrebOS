OBJECTS = $(BUILD_DIR)/loader.o $(BUILD_DIR)/io.o $(BUILD_DIR)/kmain.o $(BUILD_DIR)/fb.o $(BUILD_DIR)/gdt.o \
$(BUILD_DIR)/gdt_.o $(BUILD_DIR)/interrupts_.o $(BUILD_DIR)/interrupts.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/memory.o \
$(BUILD_DIR)/memory_.o $(BUILD_DIR)/shutdown.o $(BUILD_DIR)/system.o $(BUILD_DIR)/process.o
CC = /home/mat/opt/cross/bin/i686-elf-gcc # For some reason CLion does not read PATH entry where cross gcc path is set
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -g
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -f elf -F dwarf -g
KLIB_OBJECTS = $(shell find clib/build -name *.o)

BUILD_DIR=build
PROGRAM_BUILD_DIR=program/build
PROGRAM2_BUILD_DIR=program2/build
KLIB_BUILD_DIR=klib/build
CLIB_BUILD_DIR=clib/build

OUT_NAME=kernel
OUT_BIN=$(OUT_NAME).elf

OS_NAME=os
OS_ISO=$(OS_NAME).iso

GRUB_TIMEOUT=0

all: $(OS_ISO)

.PHONY: program program2 clib

directories:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(PROGRAM_BUILD_DIR)
	mkdir -p $(KLIB_BUILD_DIR)
	mkdir -p $(CLIB_BUILD_DIR)

program:
	make -C program

program2:
	make -C program2

clib:
	make -C clib


kernel.elf: directories $(OBJECTS) clib
	echo $(OBJECTS)
	echo $(KLIB_OBJECTS)
	ld $(LDFLAGS) $(OBJECTS) -Iclib/ $(KLIB_OBJECTS) -o $(BUILD_DIR)/kernel.elf

$(OS_ISO): kernel.elf program program2
#	Create directories
	mkdir -p isodir
	mkdir -p isodir/boot
	mkdir -p isodir/boot/grub
	mkdir -p isodir/modules

	echo "set timeout=$(GRUB_TIMEOUT)" > grub.cfg
	echo "set default=0" >> grub.cfg
	# uncomment the following lines to enable serial debugging. use it with -serial file:serial.log in qemu
	#echo "set debug=all" >> grub.cfg
	#echo "serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1" >> grub.cfg
	#echo "terminal_output serial" >> grub.cfg
	echo menuentry \"$(OUT_NAME)\" { >> grub.cfg
	echo "	multiboot /boot/$(OUT_BIN)" >> grub.cfg
	echo "	module /modules/program" >> grub.cfg
	echo "	module /modules/program2" >> grub.cfg
	echo } >> grub.cfg

	cp $(BUILD_DIR)/$(OUT_BIN) isodir/boot/$(OUT_BIN)
	cp $(PROGRAM_BUILD_DIR)/program isodir/modules/program
	cp $(PROGRAM2_BUILD_DIR)/program2 isodir/modules/program2
	cp grub.cfg isodir/boot/grub/grub.cfg

	echo "Building ISO..."
	grub-mkrescue -o $(OS_ISO) isodir

run: $(OS_ISO)
#	bochs -f bochsrc.txt -q
	qemu-system-i386 -device isa-debug-exit -cdrom $(OS_ISO)

debug: $(OS_ISO)
	qemu-system-i386 -device isa-debug-exit -cdrom $(OS_ISO) -gdb tcp::26000 -S

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf *.o $(OUT_BIN) $(OS_ISO)
	rm -rf isodir
	rm -rf $(BUILD_DIR)/*
	rm -rf $(OS_ISO)
	make -C program clean
	make -C program2 clean
	make -C klib clean
	make -C clib clean