OBJECTS = $(BUILD_DIR)/loader.o $(BUILD_DIR)/io.o $(BUILD_DIR)/kmain.o $(BUILD_DIR)/fb.o $(BUILD_DIR)/gdt.o $(BUILD_DIR)/gdt_.o $(BUILD_DIR)/interrupts_.o $(BUILD_DIR)/interrupts.o $(BUILD_DIR)/keyboard.o $(BUILD_DIR)/memory.o $(BUILD_DIR)/memory_.o
CC = gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -g
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -f elf -F dwarf -g
BUILD_DIR=build

OUT_NAME=kernel
OUT_BIN=$(OUT_NAME).elf

OS_NAME=os
OS_ISO=$(OS_NAME).iso

GRUB_TIMEOUT=0

all: $(OS_ISO)

program:
	nasm -f bin -g program.s -o $(BUILD_DIR)/program

kernel.elf: $(OBJECTS)
	mkdir -p build
	ld $(LDFLAGS) $(OBJECTS) -o $(BUILD_DIR)/kernel.elf

$(OS_ISO): kernel.elf program
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
	echo } >> grub.cfg

	cp $(BUILD_DIR)/$(OUT_BIN) isodir/boot/$(OUT_BIN)
	cp $(BUILD_DIR)/program isodir/modules/program
	cp grub.cfg isodir/boot/grub/grub.cfg

	echo "Building ISO..."
	grub-mkrescue -o $(OS_ISO) isodir

run: $(OS_ISO)
#	bochs -f bochsrc.txt -q
	qemu-system-i386 -cdrom $(OS_ISO)

debug: $(OS_ISO)
	qemu-system-i386 -cdrom $(OS_ISO) -gdb tcp::26000 -S

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@
clean:
	rm -rf *.o $(OUT_BIN) $(OS_ISO)
	rm -rf isodir
	rm -rf $(BUILD_DIR)/*
	rm -rf $(OS_ISO)