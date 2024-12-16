OBJECTS = $(BUILD_DIR)/libkapi.so
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-g -fno-exceptions -fno-rtti
libgcc=$(shell i686-elf-gcc $(CFLAGS) -print-libgcc-file-name)
LIBFLAGS = -fPIC -Wl,-shared
BUILD_DIR=build

all: kapi

.PHONY: kapi


kapi: $(OBJECTS)

# static
$(BUILD_DIR)/libkapi.a: syscalls.cpp
	$(CC) $(CFLAGS) -c $< -o $@ $(libgcc)

#dynamic
$(BUILD_DIR)/libkapi.so: syscalls.cpp
	$(CC) $(CFLAGS) $(LIBFLAGS) $< -o $@ $(libgcc)

clean:
	rm -rf build/*