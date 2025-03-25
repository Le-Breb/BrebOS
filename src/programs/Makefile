AS=nasm
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
PROGRAMS=$(shell find . -maxdepth 1 -type d | grep -v $(BUILD_DIR) | grep -v '^\.$$')

all: $(BUILD_DIR)/.dir_timestamp start_program.o $(PROGRAMS)
.PHONY: $(PROGRAMS)

$(BUILD_DIR)/.dir_timestamp:
	@mkdir -p $(BUILD_DIR)
	@touch $(BUILD_DIR)/.dir_timestamp

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