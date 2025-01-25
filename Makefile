# This Makefile compiles everything under $(SRC_DIR)/kernel and links everything (compiled objects + shell + libc...)
# and eventually builds the final ISO
# Compilation of files under $(SRC_DIR)/kernel could theoretically be delegated to a dedicated sub Makefile,
# but then CLion wouldn't parse correctly the project structure :/

SRC_DIR=src
SRC=$(shell cd $(SRC_DIR)/kernel; find . -name '*.cpp' -o -name '*.s' | sed 's|^\./||')
OBJECTS = $(patsubst %.cpp, $(KERNEL_BUILD_DIR)/%.o, $(filter %.cpp, $(SRC))) \
          $(patsubst %.s, $(KERNEL_BUILD_DIR)/%.o, $(filter %.s, $(SRC)))
CC = i686-elf-gcc
CFLAGS = -O0 -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
CPPFLAGS=-I$(SRC_DIR)/libc
libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
CRTI_OBJ=$(GCC_BUILD_DIR)/crti.o
CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=$(GCC_BUILD_DIR)/crtn.o
OBJ_LIST=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJECTS) $(CRTEND_OBJ) $(CRTN_OBJ)
INTERNAL_OBJS=$(CRTI_OBJ) $(OBJECTS) $(CRTN_OBJ)
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -O0 -f elf -F dwarf -g
LIBC=$(LIBC_BUILD_DIR)/libc.a
LD=ld

BUILD_DIR=build
SHELL_BUILD_DIR=$(SRC_DIR)/shell/build
PROGRAM2_BUILD_DIR=$(SRC_DIR)/program2/build
LIBC_BUILD_DIR=$(SRC_DIR)/libc/build
LIBDYNLK_BUILD_DIR=$(SRC_DIR)/libdynlk/build
GCC_BUILD_DIR=$(SRC_DIR)/gcc/build
KERNEL_BUILD_DIR=$(SRC_DIR)/kernel/build

OUT_NAME=kernel
OUT_BIN=$(OUT_NAME).elf

OS_NAME=os
OS_ISO=$(OS_NAME).iso

GRUB_TIMEOUT=0

all: $(OS_ISO)

.PHONY: shell program2 libc libdynlk

$(CRTI_OBJ):
	make -C src/gcc
$(CRTN_OBJ):
	make -C src/gcc

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(SHELL_BUILD_DIR)
	@mkdir -p $(PROGRAM2_BUILD_DIR)
	@mkdir -p $(LIBC_BUILD_DIR)
	@mkdir -p $(LIBDYNLK_BUILD_DIR)
	@mkdir -p $(GCC_BUILD_DIR)
	@mkdir -p $(KERNEL_BUILD_DIR)

shell:
	make -C $(SRC_DIR)/shell

program2:
	make -C $(SRC_DIR)/program2

libdynlk:
	make -C $(SRC_DIR)/libdynlk

libc:
	make -C $(SRC_DIR)/libc

$(KERNEL_BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@
$(KERNEL_BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

gcc:
	make -C $(SRC_DIR)/gcc

kernel.elf: directories $(INTERNAL_OBJS) libc gcc
	ld $(LDFLAGS) $(OBJ_LIST) -Ilibc/ $(LIBC) -o $(BUILD_DIR)/kernel.elf $(libgcc)

$(OS_ISO): kernel.elf shell program2 libdynlk
	@#Create directories
	@mkdir -p isodir
	@mkdir -p isodir/boot
	@mkdir -p isodir/boot/grub
	@mkdir -p isodir/modules

	@echo Creating disk
	@#qemu-img create -f raw disk_image.img 1M
	@dd if=/dev/zero of=disk_image.img bs=1M count=35 # Can't go a lot lower than that, otherwise drive would be interpreted as FAT16
	@#Install FAT32 on it
	@mkfs.vfat -F 32 -v disk_image.img
	@#cp disk_image.img2 disk_image.img
	@echo Populating disk
	@mmd -i disk_image.img ::/fold
	@mmd -i disk_image.img ::/fold2
	@mmd -i disk_image.img ::/bin
	@mcopy -i disk_image.img $(SHELL_BUILD_DIR)/shell ::/bin
	@mcopy -i disk_image.img $(PROGRAM2_BUILD_DIR)/program2 ::/bin
	@mcopy -i disk_image.img $(LIBC_BUILD_DIR)/libc.so ::/bin
	@mcopy -i disk_image.img $(LIBDYNLK_BUILD_DIR)/libdynlk.so ::/bin

	@echo "set timeout=$(GRUB_TIMEOUT)" > grub.cfg
	@echo "set default=0" >> grub.cfg
	@# uncomment the following lines to enable serial debugging. use it with -serial file:serial.log in qemu
	@#echo "set debug=all" >> grub.cfg
	@#echo "serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1" >> grub.cfg
	@#echo "terminal_output serial" >> grub.cfg
	@echo menuentry \"$(OUT_NAME)\" { >> grub.cfg
	@echo "	multiboot /boot/$(OUT_BIN)" >> grub.cfg
	@#echo "	module /modules/shell" >> grub.cfg
	@#echo "	module /modules/program2" >> grub.cfg
	@#echo "	module /modules/libdynlk.so" >> grub.cfg
	@#echo "	module /modules/libkapi.so" >> grub.cfg
	@echo } >> grub.cfg

	@cp $(BUILD_DIR)/$(OUT_BIN) isodir/boot/$(OUT_BIN)
	@cp $(SHELL_BUILD_DIR)/shell isodir/modules/shell
	@cp $(PROGRAM2_BUILD_DIR)/program2 isodir/modules/program2
	@cp $(LIBDYNLK_BUILD_DIR)/libdynlk.so isodir/modules/libdynlk.so
	@cp $(LIBC_BUILD_DIR)/libc.so isodir/modules/libc.so
	@cp grub.cfg isodir/boot/grub/grub.cfg

	@echo "Building ISO..."
	@grub-mkrescue -o $(OS_ISO) isodir

run: $(OS_ISO)
#	bochs -f bochsrc.txt -q
	qemu-system-i386 -device isa-debug-exit -cdrom $(OS_ISO) -drive file=disk_image.img,format=raw,if=ide,index=0 -boot d

debug: $(OS_ISO)
	qemu-system-i386 -device isa-debug-exit -cdrom $(OS_ISO) -gdb tcp::26000 -S -drive file=disk_image.img,format=raw,if=ide,index=0 -boot d

clean:
	rm -rf *.o $(OUT_BIN) $(OS_ISO)
	rm -rf isodir
	rm -rf $(BUILD_DIR)/*
	rm -rf $(KERNEL_BUILD_DIR)/*
	rm -rf $(OS_ISO)
	make -C $(SRC_DIR)/shell clean
	make -C $(SRC_DIR)/program2 clean
	make -C $(SRC_DIR)/libc clean
	make -C $(SRC_DIR)/libdynlk clean
	make -C $(SRC_DIR)/gcc/ clean