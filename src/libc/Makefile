SRC=$(shell find -name '*.cpp')
OBJECTS=$(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRC))
libgcc=$(shell $(CC) $(CFLAGS) -print-libgcc-file-name)
CC = i686-brebos-gcc
CFLAGS = -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-fno-exceptions -fno-rtti -fPIC
GCC_BUILD_DIR=../gcc/build
CRTI_OBJ=$(GCC_BUILD_DIR)/crti.o
CRTBEGIN_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ:=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ=$(GCC_BUILD_DIR)/crtn.o
OBJ_LIST=$(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJECTS) $(CRTEND_OBJ) $(CRTN_OBJ)
AS = nasm
ASFLAGS = -f elf -F dwarf
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0 -g
ASFLAGS += -O0 -g
LDFLAGS += -g
endif
BUILD_DIR=build
LIB=$(BUILD_DIR)/libc.so
LIBFLAGS = -fPIC -Wl,-shared -pie
CPPFLAGS=-I../libc

all: gcc $(BUILD_DIR)/libc.so $(BUILD_DIR)/libc.a

gcc:
	+make -C ../gcc

$(BUILD_DIR)/libc.so: $(BUILD_DIR)/.dir_timestamp $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBFLAGS) -o $@ $(OBJ_LIST) $(libgcc)

$(BUILD_DIR)/libc.a: $(BUILD_DIR)/.dir_timestamp $(OBJECTS)
	ar rcs $@ $(OBJECTS) $(libgcc)

$(BUILD_DIR)/.dir_timestamp: 
	@mkdir -p $(BUILD_DIR)
	@touch $(BUILD_DIR)/.dir_timestamp

$(BUILD_DIR)/%.o: %.cpp
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/%.o: %.s
	$(AS) $(ASFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)
