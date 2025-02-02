AS=nasm
ASFLAGS = -f elf -F dwarf -g
ifdef RELEASE
CFLAGS += -O3
ASFLAGS += -O3
else
CFLAGS += -O0
ASFLAGS += -O0
endif

BUILD_DIR=build
PROGRAMS=$(shell find . -maxdepth 1 -type d | grep -v $(BUILD_DIR) | grep -v '^\.$$')

all: directories start_program.o $(PROGRAMS)
.PHONY: $(PROGRAMS)

directories:
	@mkdir -p $(BUILD_DIR)

start_program.o: start_program.s
	$(AS) $(ASFLAGS) -o $@ $<

$(PROGRAMS):
	@cp ./program_Makefile $@/Makefile
	$(MAKE) -C $@

clean:
	@for prog in $(PROGRAMS); do \
		make -C $$prog clean; \
	done
	rm -rf start_program.o
	rm -rf $(BUILD_DIR)