OBJECTS = $(BUILD_DIR)/program.o $(BUILD_DIR)/start_program.o
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -g
INCLUDE = -I../klib/build
LDFLAGS = -T link_program.ld -melf_i386 -g
AS = nasm
ASFLAGS = -f elf -F dwarf -g
BUILD_DIR=build

all: program

.PHONY: program

klib:
	make -C ../klib

program: $(OBJECTS) klib
	ld $(LDFLAGS) -o $(BUILD_DIR)/program $(INCLUDE) $(OBJECTS) ../klib/build/*.a

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*