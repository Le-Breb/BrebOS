OBJS = program.o start_program.o
OBJECTS= $(OBJS:%.o=$(BUILD_DIR)/%.o)
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
libgcc=$(shell i686-elf-gcc $(CFLAGS) -print-libgcc-file-name)
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
	ld $(LDFLAGS) -o $(BUILD_DIR)/program $(INCLUDE) $(OBJECTS) ../klib/build/*.a $(libgcc)

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*