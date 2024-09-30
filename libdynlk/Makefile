OBJECTS = $(BUILD_DIR)/libdynlk.so
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror -g
LIBFLAGS = -fPIC -Wl,-shared
BUILD_DIR=build

all: $(OBJECTS)

$(BUILD_DIR)/%.so: %.c
	$(CC) $(CFLAGS) $(LIBFLAGS) $< -o $@

clean:
	rm -rf build/*