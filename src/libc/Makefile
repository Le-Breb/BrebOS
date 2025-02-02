SRC=$(shell find -name '*.cpp')
OBJECTS=$(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRC))
libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
CC = i686-elf-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-g -fno-exceptions -fno-rtti -fPIC
GCC_BUILD_DIR=../gcc/build
CRTI_OBJ=$(GCC_BUILD_DIR)/crti.o
CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=$(GCC_BUILD_DIR)/crtn.o
OBJ_LIST=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJECTS) $(CRTEND_OBJ) $(CRTN_OBJ)
AS = nasm
ASFLAGS = -f elf -F dwarf -g
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0
ASFLAGS += -O0
endif
BUILD_DIR=build
LIB=$(BUILD_DIR)/libc.so
LIBFLAGS = -fPIC -Wl,-shared -pie
CPPFLAGS=-I../libc

all: LIB

LIB: $(OBJECTS)
		$(CC) $(CFLAGS) $(LIBFLAGS) -o $(BUILD_DIR)/libc.so $(OBJ_LIST) $(libgcc)
	    ar rcs $(BUILD_DIR)/libc.a $(OBJECTS) $(libgcc)

.PHONY: libc

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf build/*
