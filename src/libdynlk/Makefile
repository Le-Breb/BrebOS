OBJECTS = $(BUILD_DIR)/libdynlk.o
LIB = $(BUILD_DIR)/libdynlk.so
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-g -fno-exceptions -fno-rtti
AS=nasm
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0
ASFLAGS += -O0
endif
LIBFLAGS = -fPIC -Wl,-shared
BUILD_DIR=build

all: $(LIB)

$(LIB): $(OBJECTS)
	ld -m elf_i386 -shared -o $(LIB) $(OBJECTS)

$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) -f elf32 -o $@ $<

clean:
	rm -rf build/*