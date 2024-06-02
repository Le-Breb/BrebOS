#include "gdt.h"

tss_entry_t tss;

void gdt_set_entry(struct gdt_entry* gdt, unsigned int num, unsigned int base, unsigned int limit, char access,
				   char granularity)
{
	gdt[num].base_low = (base & 0xFFFF); // NOLINT(*-narrowing-conversions)
	gdt[num].base_middle = (base >> 16) & 0xFF; // NOLINT(*-narrowing-conversions)
	gdt[num].base_high = (base >> 24) & 0xFF; // NOLINT(*-narrowing-conversions)

	gdt[num].limit_low = limit & 0xFFFF; // NOLINT(*-narrowing-conversions)
	gdt[num].granularity = (limit >> 16) & 0x0F; // NOLINT(*-narrowing-conversions)

	gdt[num].granularity |= (granularity & 0xF0); // NOLINT(*-narrowing-conversions)
	gdt[num].access = access;
}

void setup_tss(struct gdt_entry* gdt, unsigned int gdt_entry, unsigned int ss0, unsigned int esp0)
{
	unsigned int base = (unsigned int) &tss;
	unsigned int limit = base + sizeof(tss_entry_t);

	gdt_set_entry(gdt, gdt_entry, base, limit, (char) TSS_SEGMENT_ACCESS, 0x00);

	for (unsigned int i = 0; i < sizeof(tss_entry_t); ++i)
		*(((unsigned char*) &tss) + i) = 0;

	tss.ss0 = ss0;
	tss.esp0 = esp0;

	tss.cs = 0x0b;
	tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x13;
}

void set_tss_kernel_stack(unsigned int esp0)
{
	tss.esp0 = esp0;
}

void gdt_init(gdt_descriptor_t* gdt_descriptor, gdt_entry_t* gdt)
{
	// Set up the GDT descriptor
	gdt_descriptor->size = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
	gdt_descriptor->address = (void*) gdt;

	// Set up the GDT entries
	// Null segment
	gdt_set_entry(gdt, 0, 0, 0, 0, 0);

	// Kernel code segment
	gdt_set_entry(gdt, 1, 0, 0xFFFFFFFF, (char) K_CODE_SEGMENT_ACCESS, (char) GRANULARITY);

	// Kernel data segment
	gdt_set_entry(gdt, 2, 0, 0xFFFFFFFF, (char) K_DATA_SEGMENT_ACCESS, (char) GRANULARITY);

	// User code segment
	gdt_set_entry(gdt, 3, 0, 0xFFFFFFFF, (char) U_CODE_SEGMENT_ACCESS, (char) GRANULARITY);

	// User data segment
	gdt_set_entry(gdt, 4, 0, 0xFFFFFFFF, (char) U_DATA_SEGMENT_ACCESS, (char) GRANULARITY);

	// TSS
	setup_tss(gdt, 5, 0x10, 0x00);

	// Load the GDT
	load_gdt(gdt_descriptor);
	load_tss();
}