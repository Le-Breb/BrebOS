OBJS = loader.o io.o kmain.o fb.o gdt.o gdt_.o interrupts_.o interrupts.o keyboard.o memory.o memory_.o shutdown.o \
system.o process.o syscalls.o list.o PIT.o PIC.o IDT.o ELF.o scheduler.o
OBJECTS= $(OBJS:%.o=$(BUILD_DIR)/%.o)
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
libgcc=$(shell i686-elf-gcc $(CFLAGS) -print-libgcc-file-name)
CRTI_OBJ=$(BUILD_DIR)/crti.o
CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=$(BUILD_DIR)/crtn.o
OBJ_LIST=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJECTS) $(CRTEND_OBJ) $(CRTN_OBJ)
INTERNAL_OBJS=$(CRTI_OBJ) $(OBJECTS) $(CRTN_OBJ)
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -f elf -F dwarf -g
KLIB_OBJECTS = $(shell find clib/build -name *.o)

BUILD_DIR=build
PROGRAM_BUILD_DIR=program/build
PROGRAM2_BUILD_DIR=program2/build
KLIB_BUILD_DIR=klib/build
CLIB_BUILD_DIR=clib/build
LIBDYNLK_BUILD_DIR=libdynlk/build

OUT_NAME=kernel
OUT_BIN=$(OUT_NAME).elf

OS_NAME=os
OS_ISO=$(OS_NAME).iso

GRUB_TIMEOUT=0

all: $(OS_ISO)

.PHONY: program program2 clib libdynlk

directories:
	mkdir -p $(BUILD_DIR)
	mkdir -p $(PROGRAM_BUILD_DIR)
	mkdir -p $(PROGRAM2_BUILD_DIR)
	mkdir -p $(KLIB_BUILD_DIR)
	mkdir -p $(CLIB_BUILD_DIR)
	mkdir -p $(LIBDYNLK_BUILD_DIR)

program:
	make -C program

program2:
	make -C program2

libdynlk:
	make -C libdynlk

clib:
	make -C clib


kernel.elf: directories $(INTERNAL_OBJS) clib
	ld $(LDFLAGS) $(OBJ_LIST) -Iclib/ $(KLIB_OBJECTS) -o $(BUILD_DIR)/kernel.elf $(libgcc)

$(OS_ISO): kernel.elf program program2 libdynlk
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
	echo "	module /modules/libdynlk.so" >> grub.cfg
	echo "	module /modules/libsyscalls.so" >> grub.cfg
	echo } >> grub.cfg

	cp $(BUILD_DIR)/$(OUT_BIN) isodir/boot/$(OUT_BIN)
	cp $(PROGRAM_BUILD_DIR)/program isodir/modules/program
	cp $(PROGRAM2_BUILD_DIR)/program2 isodir/modules/program2
	cp $(LIBDYNLK_BUILD_DIR)/libdynlk.so isodir/modules/libdynlk.so
	cp $(KLIB_BUILD_DIR)/libsyscalls.so isodir/modules/libsyscalls.so
	cp grub.cfg isodir/boot/grub/grub.cfg

	echo "Building ISO..."
	grub-mkrescue -o $(OS_ISO) isodir

run: $(OS_ISO)
#	bochs -f bochsrc.txt -q
	qemu-system-i386 -device isa-debug-exit -cdrom $(OS_ISO)

debug: $(OS_ISO)
	qemu-system-i386 -device isa-debug-exit -cdrom $(OS_ISO) -gdb tcp::26000 -S

$(BUILD_DIR)/%.o: %.cpp
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
	make -C libdynlk clean
