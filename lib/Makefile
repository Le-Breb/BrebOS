BUILD_DIR=build
LIB_OBJECTS = $(BUILD_DIR)/string.o
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -c -g
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -f elf -F dwarf -g

all: $(OBJECTS)

$(BUILD_DIR)/%.o: lib/%.c
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: lib/%.s
	$(AS) $(ASFLAGS) $< -o $@