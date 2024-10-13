OBJECTS = $(BUILD_DIR)/syscalls.a $(BUILD_DIR)/libsyscalls.so
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -g
libgcc=$(shell i686-elf-gcc $(CFLAGS) -print-libgcc-file-name)
LIBFLAGS = -fPIC -Wl,-shared
BUILD_DIR=build

all: klib

.PHONY: klib


klib: $(OBJECTS)

# static
$(BUILD_DIR)/syscalls.a: syscalls.c
	$(CC) $(CFLAGS) -c $< -o $@

#dynamic
$(BUILD_DIR)/libsyscalls.so: syscalls.c
	$(CC) $(CFLAGS) $(libgcc) $(LIBFLAGS) $< -o $@

clean:
	rm -rf build/*