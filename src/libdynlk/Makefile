OBJECTS = $(BUILD_DIR)/libdynlk.o
LIB = $(BUILD_DIR)/libdynlk.so
CC = i686-brebos-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-g -fno-exceptions -fno-rtti
AS=nasm
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0 -g
ASFLAGS += -O0 -g
LDFLAGS += -g
endif
LIBFLAGS = -fPIC -Wl,-shared
BUILD_DIR=build

all: $(LIB)

$(BUILD_DIR)/.dir_timestamp:
	@mkdir -p $(BUILD_DIR)
	@touch $(BUILD_DIR)/.dir_timestamp

$(LIB): $(BUILD_DIR)/.dir_timestamp $(OBJECTS)
	i686-brebos-ld -m elf_i386 -shared -o $(LIB) $(OBJECTS)

$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) -f elf32 -o $@ $<

clean:
	rm -rf $(BUILD_DIR)