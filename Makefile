# This Makefile compiles everything under $(SRC_DIR)/kernel and links everything (compiled objects + libc)
# and eventually builds the final ISO
# This Makefile also fills /bin in disk_image.img with every program under $(SRC_DIR)/programs
# Compilation of files under $(SRC_DIR)/kernel could theoretically be delegated to a dedicated sub Makefile,
# but then CLion wouldn't parse correctly the project structure :/

RED=\033[0;31m
GREEN=\033[0;32m
YELLOW=\033[0;33m
BLUE=\033[0;34m
MAGENTA=\033[0;35m
CYAN=\033[0;36m
WHITE=\033[0;37m
RESET=\033[0m

SRC_DIR=src
SRC=$(shell cd $(SRC_DIR)/kernel; find . -name '*.cpp' -o -name '*.s' | sed 's|^\./||')
OBJECTS = $(patsubst %.cpp, $(KERNEL_BUILD_DIR)/%.o, $(filter %.cpp, $(SRC))) \
          $(patsubst %.s, $(KERNEL_BUILD_DIR)/%.o, $(filter %.s, $(SRC)))

CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -fno-exceptions -fno-rtti
AS = nasm
ASFLAGS = -f elf -F dwarf
LDFLAGS = -T link.ld -melf_i386
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0 -g
ASFLAGS += -O0 -g
LDFLAGS += -g
endif

libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
CRTI_OBJ=$(GCC_BUILD_DIR)/crti.o
CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=$(GCC_BUILD_DIR)/crtn.o
OBJ_LIST=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJECTS) $(CRTEND_OBJ) $(CRTN_OBJ)
INTERNAL_OBJS=$(CRTI_OBJ) $(OBJECTS) $(CRTN_OBJ)

CPPFLAGS=-I$(SRC_DIR)/libc
LIBC=$(LIBC_BUILD_DIR)/libc.a
LD=ld

BUILD_DIR=build
LIBC_BUILD_DIR=$(SRC_DIR)/libc/build
LIBDYNLK_BUILD_DIR=$(SRC_DIR)/libdynlk/build
GCC_BUILD_DIR=$(SRC_DIR)/gcc/build
KERNEL_BUILD_DIR=$(SRC_DIR)/kernel/build

OUT_NAME=kernel
OUT_BIN=$(OUT_NAME).elf
OS_NAME=os
OS_ISO=$(OS_NAME).iso

GRUB_TIMEOUT=0

all: init $(OS_ISO)

.PHONY: libc libdynlk programs

export PATH := $(HOME)/opt/cross/bin/:$(PATH)

init:
	@echo "$(CYAN)Compiling$(WHITE)"

$(CRTI_OBJ):
	make -C src/gcc
$(CRTN_OBJ):
	make -C src/gcc

directories:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(KERNEL_BUILD_DIR)

libdynlk:
	make -C $(SRC_DIR)/libdynlk

libc:
	make -C $(SRC_DIR)/libc

programs: directories libc libdynlk
	make -C $(SRC_DIR)/programs

$(KERNEL_BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@
$(KERNEL_BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

gcc:
	make -C $(SRC_DIR)/gcc

kernel.elf: directories $(INTERNAL_OBJS) libc gcc
	ld $(LDFLAGS) $(OBJ_LIST) $(CPPFLAGS) $(LIBC) -o $(BUILD_DIR)/kernel.elf $(libgcc)

$(OS_ISO): kernel.elf libdynlk programs
	@#Create directories
	@mkdir -p isodir
	@mkdir -p isodir/boot
	@mkdir -p isodir/boot/grub
	@mkdir -p isodir/modules

	@echo "$(CYAN)Creating disk$(WHITE)"
	@#qemu-img create -f raw disk_image.img 1M
	@dd if=/dev/zero of=disk_image.img bs=1M count=35 # Can't go a lot lower than that, otherwise drive would be interpreted as FAT16
	@#Install FAT32 on it
	@mkfs.vfat -F 32 -v disk_image.img
	@#cp disk_image.img2 disk_image.img
	@echo "$(CYAN)Populating disk$(WHITE)"
	@mmd -i disk_image.img ::/fold
	@mmd -i disk_image.img ::/fold2
	@mmd -i disk_image.img ::/bin
	@mcopy -i disk_image.img $(LIBC_BUILD_DIR)/libc.so ::/bin
	@mcopy -i disk_image.img $(LIBDYNLK_BUILD_DIR)/libdynlk.so ::/bin
	@for prog in $(shell find $(SRC_DIR)/programs/build -type f); do \
    		mcopy -i disk_image.img $$prog ::/bin; \
	done

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
	@cp grub.cfg isodir/boot/grub/grub.cfg

	@echo "$(CYAN)"Building ISO..."$(WHITE)"
	@grub-mkrescue -o $(OS_ISO) isodir

run: $(OS_ISO)
	@#	bochs -f bochsrc.txt -q
	@echo "$(CYAN)Initializing NAT$(WHITE)"
	@sudo ./network_utils/setup.sh
	qemu-system-i386 -device isa-debug-exit -cdrom $(OS_ISO) -drive file=disk_image.img,format=raw,if=ide,index=0 -boot d -netdev tap,id=net0,ifname=tap0,script=no,downscript=no  -device e1000,netdev=net0  -object filter-dump,id=dump0,netdev=net0,file=vm_traffic.pcap || true
	@echo "$(CYAN)Restoring default network configuration...$(WHITE)"
	@sudo ./network_utils/cleanup.sh
	@echo "$(CYAN)Done$(WHITE)"
	@# -device isa-debug-exit -cdrom os.iso -gdb tcp::
# -S -drive file=disk_image.img,format=raw,if=ide,index=0 -boot d -device e1000,netdev=net0 -netdev user,id=net0 -object filter-dump,id=dump0,netdev=net0,file=vm_traffic.pcap
	@# -device isa-debug-exit -cdrom os.iso -gdb tcp::26000 -S -drive file=disk_image.img,format=raw,if=ide,index=0 -boot d -device e1000,netdev=net0 -netdev tap,id=net0,ifname=tap0,script=no,downscript=no -object filter-dump,id=dump0,netdev=net0,file=vm_traffic.pcap

clean:
	rm -rf *.o $(OUT_BIN) $(OS_ISO) isodir $(BUILD_DIR) $(KERNEL_BUILD_DIR)
	make -C $(SRC_DIR)/libc clean
	make -C $(SRC_DIR)/libdynlk clean
	make -C $(SRC_DIR)/gcc/ clean
	make -C $(SRC_DIR)/programs/ clean
