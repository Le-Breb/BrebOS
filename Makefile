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

BUILD_DIR=build
SRC_DIR=src
SYSROOT=./sysroot
NEWLIB_INCLUDE_DIR=$(SYSROOT)/usr/include/
TOOLCHAIN_DIR=./toolchain
GCC_INCLUDE_DIR=$(TOOLCHAIN_DIR)/gcc/lib/gcc/i686-brebos/15.1.0/include
LIBC_BUILD_DIR=$(SRC_DIR)/libc/build
LIBK_BUILD_DIR=$(SRC_DIR)/libk/build
LIBDYNLK_BUILD_DIR=$(SRC_DIR)/libdynlk/build
GCC_BUILD_DIR=$(SRC_DIR)/gcc/build
KERNEL_BUILD_DIR=$(SRC_DIR)/kernel/build
PROGRAMS_BUILD_DIR=$(SRC_DIR)/programs/build

SRC=$(shell cd $(SRC_DIR)/kernel; find . -name '*.cpp' -o -name '*.s' | sed 's|^\./||')
OBJECTS = $(patsubst %.cpp, $(KERNEL_BUILD_DIR)/%.o, $(filter %.cpp, $(SRC))) \
          $(patsubst %.s, $(KERNEL_BUILD_DIR)/%.o, $(filter %.s, $(SRC)))
DEPS=$(OBJECTS:.o=.d)
-include $(DEPS)

CC = i686-brebos-gcc
CFLAGS = -m32 -nostdlib -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -fno-exceptions -fno-rtti -MMD -MP
AS = nasm
ASFLAGS = -f elf -F dwarf
LDFLAGS = -T link.ld -melf_i386
ifdef RELEASE
CFLAGS += -O3 -g
ASFLAGS += -O3 -g
LDFLAGS += -g
else
CFLAGS += -O0 -g
ASFLAGS += -O0 -g
LDFLAGS += -g
endif
ifdef PROFILING
	CFLAGS += -finstrument-functions -DPROFILING
endif

CC_PATH=$(CURDIR)/toolchain/gcc/bin
libgcc=$(shell $(CC_PATH)/$(CC) $(CFLAGS) -print-libgcc-file-name)
CRTI_OBJ=$(GCC_BUILD_DIR)/crti.o
CRTBEGIN_OBJ:=$(shell $(CC_PATH)/$(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC_PATH)/$(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=$(GCC_BUILD_DIR)/crtn.o
OBJ_LIST=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJECTS) $(CRTEND_OBJ) $(CRTN_OBJ)
INTERNAL_OBJS=$(CRTI_OBJ) $(OBJECTS) $(CRTN_OBJ)
programs=$(PROGRAMS_BUILD_DIR)
CPPFLAGS=-I$(SRC_DIR)/libc
libc=$(LIBC_BUILD_DIR)/libc.a
libk=$(LIBC_BUILD_DIR)/libk.a
libdynlk=$(LIBDYNLK_BUILD_DIR)/libdynlk.o

programs_sources=$(shell find $(SRC_DIR)/programs -type f -name '*.cpp') $(SRC_DIR)/programs/start_program.s
libc_sources=$(shell find $(SRC_DIR)/libc -type f -name '*.cpp')
libk_sources=$(shell find $(SRC_DIR)/libk -type f -name '*.cpp')
libdynlk_sources=$(shell find $(SRC_DIR)/libdynlk -type f -name '*.cpp')

LD=ld

OUT_NAME=kernel
OUT_BIN=$(OUT_NAME).elf
OS_NAME=os
OS_ISO=$(OS_NAME).iso

GRUB_TIMEOUT=0

FONT_FILE=Lat15-VGA16.psf
FONT_OBJ= $(BUILD_DIR)/$(FONT_FILE:%.psf=%.o)

all:
	@PATH=$(CC_PATH):$(PATH) $(MAKE) all_

run:
	@PATH=$(CC_PATH):$(PATH) $(MAKE) run_

all_: init $(OS_ISO) compilation_ended
.PHONY: libc libk libdynlk programs

init:
	@echo "$(CYAN)Compiling$(WHITE)"

compilation_ended:
	@echo "$(CYAN)Compilation ended$(WHITE)"

$(CRTI_OBJ):
	+$(MAKE) -C src/gcc
$(CRTN_OBJ):
	+$(MAKE) -C src/gcc

$(BUILD_DIR)/.dir_timestamp:
	@mkdir -p $(BUILD_DIR)
	@touch $(BUILD_DIR)/.dir_timestamp
	@mkdir -p $(KERNEL_BUILD_DIR)

$(libdynlk): $(libdynlk_sources)
	+$(MAKE) -C $(SRC_DIR)/libdynlk

$(libc): $(libc_sources)
	+$(MAKE) -C $(SRC_DIR)/libc

$(libk): $(libk_sources)
	+$(MAKE) -C $(SRC_DIR)/libk

$(programs): $(programs_sources) $(libk) $(libdynlk)
	+$(MAKE) -C $(SRC_DIR)/programs

$(KERNEL_BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@
$(KERNEL_BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@
gcc:
	+$(MAKE) -C $(SRC_DIR)/gcc

$(FONT_OBJ): $(FONT_FILE)
	objcopy -I binary -O elf32-i386 -B i386 $(FONT_FILE) $(FONT_OBJ)

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/.dir_timestamp $(FONT_OBJ) $(INTERNAL_OBJS) $(libc) $(gcc)
	i686-brebos-ld $(LDFLAGS) $(OBJ_LIST) $(libc) $(FONT_OBJ) -o $(BUILD_DIR)/kernel.elf $(libgcc)

$(OS_ISO): $(BUILD_DIR)/kernel.elf $(libdynlk) $(programs)
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
	@mmd -i disk_image.img ::/downloads
	#@mcopy -i disk_image.img $(LIBC_BUILD_DIR)/libc.so ::/bin
	@mcopy -i disk_image.img $(LIBK_BUILD_DIR)/libk.so ::/bin
	@mcopy -i disk_image.img $(LIBDYNLK_BUILD_DIR)/libdynlk.so ::/bin
	@echo "this is a text file :D" | mcopy -i disk_image.img - ::/"text-file.txt"
	@for prog in $(shell find $(SRC_DIR)/programs/build -type f ! -name "*.*"); do \
    		mcopy -i disk_image.img $$prog ::/bin; \
	done

	@echo "set timeout=$(GRUB_TIMEOUT)" > grub.cfg
	@echo "set default=0" >> grub.cfg
	@# uncomment the following lines to enable serial debugging. use it with -serial file:serial.log in qemu
	@#echo "set debug=all" >> grub.cfg
	@#echo "serial --unit=0 --speed=115200 --word=8 --parity=no --stop=1" >> grub.cfg
	@#echo "terminal_output serial" >> grub.cfg
	#@echo "set gfxmode=1024x768x32\nset gfxpayload=keep\ninsmod gfxterm\ninsmod vbe\nterminal_output gfxterm" >> grub.cfg
	@echo menuentry \"$(OUT_NAME)\" { >> grub.cfg
	@echo "	multiboot2 /boot/$(OUT_BIN)" >> grub.cfg
	@#echo "	module /modules/shell" >> grub.cfg
	@#echo "	module /modules/program2" >> grub.cfg
	@#echo "	module /modules/libdynlk.so" >> grub.cfg
	@#echo "	module /modules/libkapi.so" >> grub.cfg
	@echo } >> grub.cfg

	@cp $(BUILD_DIR)/$(OUT_BIN) isodir/boot/$(OUT_BIN)
	@cp grub.cfg isodir/boot/grub/grub.cfg

	@echo "$(CYAN)"Building ISO..."$(WHITE)"
	@grub-mkrescue -o $(OS_ISO) isodir

run_: $(OS_ISO)
	@#	bochs -f bochsrc.txt -q
	@mdeltree -i disk_image.img ::/downloads/ # make sure downloads folder is cleanup up
	@mmd -i disk_image.img ::/downloads # Recreate it
	@echo "$(CYAN)Initializing NAT$(WHITE)"
	@sudo ./utils/net_setup.sh
	qemu-system-i386 -device isa-debug-exit -cdrom $(OS_ISO) -drive file=disk_image.img,format=raw,if=ide,index=0 -boot d -netdev tap,id=net0,ifname=tap0,script=no,downscript=no  -device e1000,netdev=net0  -object filter-dump,id=dump0,netdev=net0,file=vm_traffic.pcap || true
	@echo "$(CYAN)Restoring default network configuration...$(WHITE)"
	@sudo ./utils/net_cleanup.sh
	@echo "$(CYAN)Done$(WHITE)"
	@# -device isa-debug-exit -cdrom os.iso -gdb tcp::26000 -S -drive file=disk_image.img,format=raw,if=ide,index=0 -boot d -device e1000,netdev=net0 -netdev user,id=net0 -object filter-dump,id=dump0,netdev=net0,file=vm_traffic.pcap
	@# -device isa-debug-exit -cdrom os.iso -gdb tcp::26000 -S -drive file=disk_image.img,format=raw,if=ide,index=0 -boot d -device e1000,netdev=net0 -netdev tap,id=net0,ifname=tap0,script=no,downscript=no -object filter-dump,id=dump0,netdev=net0,file=vm_traffic.pcap
ifdef PROFILING
	@bash ./utils/run_prof.sh
endif

clean:
	rm -rf *.o $(OUT_BIN) $(OS_ISO) isodir $(BUILD_DIR) $(KERNEL_BUILD_DIR) grub.cfg
	$(MAKE) -C $(SRC_DIR)/libc clean
	$(MAKE) -C $(SRC_DIR)/libk clean
	$(MAKE) -C $(SRC_DIR)/libdynlk clean
	$(MAKE) -C $(SRC_DIR)/gcc/ clean
	$(MAKE) -C $(SRC_DIR)/programs/ clean
