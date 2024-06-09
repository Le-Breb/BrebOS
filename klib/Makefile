OBJECTS = $(BUILD_DIR)/syscalls.a
CC = /home/mat/opt/cross/bin/i686-elf-gcc # For some reason CLion does not read PATH entry where cross gcc path is set
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -g
AS = nasm
ASFLAGS = -f elf -F dwarf -g
BUILD_DIR=build

all: klib

.PHONY: klib


klib: $(OBJECTS)


$(BUILD_DIR)/%.a: %.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*