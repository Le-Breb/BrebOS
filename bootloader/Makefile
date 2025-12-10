NASM = nasm
CC = i686-elf-gcc
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy
BUILD_DIR=build

CFLAGS = -m32 -ffreestanding -Wall -Wextra
CPPFLAGS=-I./
ASFLAGS = -f elf32
LDFLAGS = -m elf_i386

ifdef RELEASE
CFLAGS += -O3 -g
ASFLAGS += -O3 -g
LDFLAGS += -g
else
CFLAGS += -O0 -g
ASFLAGS += -O0 -g
LDFLAGS += -g
endif

CPP_SRC=bootloader2.cpp ATA.cpp IO.cpp ELF.cpp ELFLoader.cpp libc.cpp malloc.cpp
ASM_SRC=kernel-entry.s io.s
CPP_OBJ=$(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CPP_SRC))
ASM_OBJ=$(patsubst %.s, $(BUILD_DIR)/%.o, $(ASM_SRC))

all: bootloader.bin bootloader2.bin

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	$(NASM) $(ASFLAGS) $< -o $@

all: bootloader.bin bootloader2.bin

bootloader.bin: bootloader.s
	$(NASM) -f bin bootloader.s -o $(BUILD_DIR)/bootloader.bin

bootloader2.elf: $(CPP_OBJ) $(ASM_OBJ)
	$(LD) $(LDFLAGS) -T link.ld $(CPP_OBJ) $(ASM_OBJ) -o $(BUILD_DIR)/bootloader2.elf

bootloader2.bin: bootloader2.elf
	$(OBJCOPY) -O binary $(BUILD_DIR)/bootloader2.elf $(BUILD_DIR)/bootloader2.bin

clean:
	$(RM) -r $(BUILD_DIR)/*