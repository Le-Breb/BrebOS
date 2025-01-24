OBJS = shell.o start_shell.o
OBJECTS= $(OBJS:%.o=$(BUILD_DIR)/%.o)
CC = i686-elf-gcc
CFLAGS = -O0 -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LDFLAGS = -T link_shell.ld -melf_i386 -g -L../libc/build/ -lc -dynamic-linker /dynlk
AS = nasm
ASFLAGS = -O0 -f elf -F dwarf -g
BUILD_DIR=build
LIBC=../libc/build/libc.a
CPPFLAGS=-I../libc

all: shell

.PHONY: shell

libc:
	make -C ../libc

shell: $(OBJECTS) libc
	ld $(LDFLAGS) -o $(BUILD_DIR)/shell $(OBJECTS) $(libgcc)

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*