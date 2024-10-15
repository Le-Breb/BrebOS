#include "gdt.h"

tss_entry_t tss;

gdt_entry_t GDT::gdt[GDT_ENTRIES];

gdt_descriptor_t GDT::gdt_descriptor;

extern "C" void load_gdt_asm_(struct gdt_descriptor* gdt);

extern "C" void load_tss_asm_();

void GDT::set_entry(unsigned int num, unsigned int base, unsigned int limit, char access, char granularity)
{
	gdt[num].base_low = (base & 0xFFFF); // NOLINT(*-narrowing-conversions)
	gdt[num].base_middle = (base >> 16) & 0xFF; // NOLINT(*-narrowing-conversions)
	gdt[num].base_high = (base >> 24) & 0xFF; // NOLINT(*-narrowing-conversions)

	gdt[num].limit_low = limit & 0xFFFF; // NOLINT(*-narrowing-conversions)
	gdt[num].granularity = (limit >> 16) & 0x0F; // NOLINT(*-narrowing-conversions)

	gdt[num].granularity |= (granularity & 0xF0); // NOLINT(*-narrowing-conversions)
	gdt[num].access = access;
}

void GDT::setup_tss(unsigned int gdt_entry, unsigned int ss0, unsigned int esp0)
{
	unsigned int base = (unsigned int) &tss;
	unsigned int limit = base + sizeof(tss_entry_t);

	set_entry(gdt_entry, base, limit, (char) TSS_SEGMENT_ACCESS, 0x00);

	for (unsigned int i = 0; i < sizeof(tss_entry_t); ++i)
		*(((unsigned char*) &tss) + i) = 0;

	tss.ss0 = ss0;
	tss.esp0 = esp0;

	tss.cs = 0x0b;
	tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x13;
}

void GDT::set_tss_kernel_stack(unsigned int esp0)
{
	tss.esp0 = esp0;
}

void GDT::init()
{
	// Set up the GDT descriptor
	gdt_descriptor.size = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
	gdt_descriptor.address = (void*) gdt;

	// Set up the GDT entries
	// Null segment
	set_entry(0, 0, 0, 0, 0);

	// Kernel code segment
	set_entry(1, 0, 0xFFFFFFFF, (char) K_CODE_SEGMENT_ACCESS, (char) GRANULARITY);

	// Kernel data segment
	set_entry(2, 0, 0xFFFFFFFF, (char) K_DATA_SEGMENT_ACCESS, (char) GRANULARITY);

	// User code segment
	set_entry(3, 0, 0xFFFFFFFF, (char) U_CODE_SEGMENT_ACCESS, (char) GRANULARITY);

	// User data segment
	set_entry(4, 0, 0xFFFFFFFF, (char) U_DATA_SEGMENT_ACCESS, (char) GRANULARITY);

	// TSS
	setup_tss(5, 0x10, 0x00);

	// Load the GDT
	GDT::load_asm();
	GDT::load_tss_asm();
}

void GDT::load_asm()
{
	load_gdt_asm_(&gdt_descriptor);
}

void GDT::load_tss_asm()
{
	load_tss_asm_();
}