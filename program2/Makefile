OBJECTS = $(BUILD_DIR)/program2.o $(BUILD_DIR)/start_program2.o
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -g
LDFLAGS = -T link_program2.ld -melf_i386 -g -L../klib/build/ -lsyscalls -dynamic-linker /dynlk
AS = nasm
ASFLAGS = -f elf -F dwarf -g
BUILD_DIR=build

all: program

.PHONY: program

klib:
	make -C ../klib

program: $(OBJECTS) klib
	ld $(LDFLAGS) -o $(BUILD_DIR)/program2 $(OBJECTS)

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*