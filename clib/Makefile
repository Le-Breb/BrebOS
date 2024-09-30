OBJECTS = $(BUILD_DIR)/string.o $(BUILD_DIR)/stdio.o $(BUILD_DIR)/cstddef.o $(BUILD_DIR)/ctype.o $(BUILD_DIR)/wchar.o \
$(BUILD_DIR)/stdint.o
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -g
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -f elf -F dwarf -g

BUILD_DIR=build

all: $(OBJECTS)

.PHONY: clib

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*