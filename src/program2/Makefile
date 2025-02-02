OBJS = program2.o start_program2.o
OBJECTS= $(OBJS:%.o=$(BUILD_DIR)/%.o)
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0
ASFLAGS += -O0
endif
libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LDFLAGS = -T link_program2.ld -melf_i386 -g -L../libc/build/ -lc -dynamic-linker /dynlk
AS = nasm
ASFLAGS = -f elf -F dwarf -g
BUILD_DIR=build
CPPFLAGS=-I../libc

all: program

.PHONY: program

libc:
	make -C ../libc

program: $(OBJECTS) libc
	ld $(LDFLAGS) -o $(BUILD_DIR)/program2 $(OBJECTS) $(libgcc)

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*