SRC=$(shell find -name '*.cpp')
OBJECTS=$(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRC))
libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
CC = i686-elf-gcc
CFLAGS = -O0 -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-g -fno-exceptions -fno-rtti -fPIC
LDFLAGS = -T link.ld -melf_i386 -g
AS = nasm
ASFLAGS = -O0 -f elf -F dwarf -g
BUILD_DIR=build
LIB=$(BUILD_DIR)/libc.so
LIBFLAGS = -fPIC -Wl,-shared
CPPFLAGS=-I../libc

all: LIB

LIB: $(OBJECTS)
		$(CC) $(CFLAGS) $(LIBFLAGS) -o $(BUILD_DIR)/libc.so $(OBJECTS) $(libgcc)
	    ar rcs $(BUILD_DIR)/libc.a $(OBJECTS) $(libgcc)

.PHONY: libc

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*