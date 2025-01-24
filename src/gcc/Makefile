SRC=$(shell find -name '*.s')
OBJECTS=$(patsubst %.s, $(BUILD_DIR)/%.o, $(SRC))
CC = i686-elf-gcc
CFLAGS = -O0 -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -O0 -f elf -F dwarf -g
BUILD_DIR=build

all: $(OBJECTS)

.PHONY: gcc

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*