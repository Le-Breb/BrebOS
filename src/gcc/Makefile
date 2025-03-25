SRC=$(shell find -name '*.s')
OBJECTS=$(patsubst %.s, $(BUILD_DIR)/%.o, $(SRC))
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -fno-exceptions -fno-rtti
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0 -g
ASFLAGS += -O0 -g
LDFLAGS += -g
endif
LDFLAGS = -T link.ld -melf_i386
AS = nasm
ASFLAGS = -f elf -F dwarf
BUILD_DIR=build

all: $(BUILD_DIR)/.dir_timestamp $(OBJECTS)

$(BUILD_DIR)/.dir_timestamp:
	@mkdir -p $(BUILD_DIR)
	@touch $(BUILD_DIR)/.dir_timestamp

.PHONY: gcc

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)