OBJS = shell.o start_shell.o
OBJECTS= $(OBJS:%.o=$(BUILD_DIR)/%.o)
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-c -g -fno-exceptions -fno-rtti
GCC_BUILD_DIR=../gcc/build
CRTI_OBJ=$(GCC_BUILD_DIR)/crti.o
CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=$(GCC_BUILD_DIR)/crtn.o
OBJ_LIST=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJECTS) $(CRTEND_OBJ) $(CRTN_OBJ)
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0
ASFLAGS += -O0
endif
libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
LDFLAGS = -T link_shell.ld -melf_i386 -g -L../libc/build/ -lc -dynamic-linker /dynlk
AS = nasm
ASFLAGS = -f elf -F dwarf -g
BUILD_DIR=build
LIBC=../libc/build/libc.a
CPPFLAGS=-I../libc

all: shell

.PHONY: shell

libc:
	make -C ../libc

shell: $(OBJECTS) libc
	ld $(LDFLAGS) -o $(BUILD_DIR)/shell $(OBJ_LIST) $(libgcc)

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*