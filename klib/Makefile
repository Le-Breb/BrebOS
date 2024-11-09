OBJECTS = $(BUILD_DIR)/libsyscalls.so
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-g -fno-exceptions -fno-rtti
libgcc=$(shell i686-elf-gcc $(CFLAGS) -print-libgcc-file-name)
LIBFLAGS = -fPIC -Wl,-shared
BUILD_DIR=build

all: klib

.PHONY: klib


klib: $(OBJECTS)

# static
$(BUILD_DIR)/syscalls.a: syscalls.cpp
	$(CC) $(CFLAGS) -c $< -o $@ $(libgcc)

#dynamic
$(BUILD_DIR)/libsyscalls.so: syscalls.cpp
	$(CC) $(CFLAGS) $(LIBFLAGS) $< -o $@ $(libgcc)

clean:
	rm -rf build/*