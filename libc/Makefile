SRC=$(shell find -name '*.cpp')
OBJECTS=$(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRC))
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -f elf -F dwarf -g
BUILD_DIR=build
LIB=$(BUILD_DIR)/libc.so
LIBFLAGS = -fPIC -Wl,-shared

all: LIB

LIB: $(OBJECTS)
	    ar rcs $(BUILD_DIR)/libc.a $(OBJECTS)

.PHONY: libc

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*