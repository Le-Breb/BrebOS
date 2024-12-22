OBJS = shell.o start_shell.o
OBJECTS= $(OBJS:%.o=$(BUILD_DIR)/%.o)
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LDFLAGS = -T link_shell.ld -melf_i386 -g -L../kapi/build/ -lkapi -dynamic-linker /dynlk
AS = nasm
ASFLAGS = -f elf -F dwarf -g
BUILD_DIR=build

all: shell

.PHONY: shell

kapi:
	make -C ../kapi

shell: $(OBJECTS) kapi
	ld $(LDFLAGS) -o $(BUILD_DIR)/shell $(OBJECTS) $(libgcc)

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*