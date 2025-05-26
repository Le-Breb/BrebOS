SRC=$(shell find -name '*.cpp')
OBJECTS=$(patsubst %.cpp, $(BUILD_DIR)/%.o, $(SRC))
CC = i686-brebos-gcc
CFLAGS = -fno-stack-protector -nostartfiles -nodefaultlibs -Wall -Wextra -Werror \
-fno-exceptions -fno-rtti -fPIC

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
LIB=$(BUILD_DIR)/libk.so
LIBFLAGS = -fPIC -Wl,-shared -pie -static-libgcc
CPPFLAGS=-I./

all: gcc $(BUILD_DIR)/libk.so $(BUILD_DIR)/libk.a

gcc:
	+make -C ../gcc

$(BUILD_DIR)/libk.so: $(BUILD_DIR)/.dir_timestamp $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBFLAGS) -o $@ $(OBJECTS) $(libgcc)

$(BUILD_DIR)/libk.a: $(BUILD_DIR)/.dir_timestamp $(OBJECTS)
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
