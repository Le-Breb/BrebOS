#ifndef INCLUDE_OS_GDT_H
#define INCLUDE_OS_GDT_H

#include <kstddef.h>

#define PL0 0x00
#define PL1 0x20
#define PL2 0x40
#define PL3 0x60
#define READ_WRITE 0x02
#define EXECUTE 0x08
#define PRESENT 0x80
#define CODE_OR_DATA 0x10
#define GRANULARITY 0xCF /* 4KB page 32bit    full address space */
#define ACCESSED 0x01

#define CODE_SEGMENT (EXECUTE | READ_WRITE | PRESENT | CODE_OR_DATA)
#define DATA_SEGMENT (READ_WRITE | PRESENT | CODE_OR_DATA)
#define K_CODE_SEGMENT_ACCESS (PL0 | CODE_SEGMENT)
#define K_DATA_SEGMENT_ACCESS (PL0 | DATA_SEGMENT)
#define U_CODE_SEGMENT_ACCESS (PL3 | CODE_SEGMENT)
#define U_DATA_SEGMENT_ACCESS (PL3 | DATA_SEGMENT)
#define TSS_SEGMENT_ACCESS (PRESENT | PL3 | EXECUTE | ACCESSED)

#define GDT_ENTRIES 6

//https://wiki.osdev.org/Global_Descriptor_Table

// Global Descriptor Table
struct gdt_descriptor
{
	unsigned short size;
	void* address;
} __attribute__((packed));

typedef struct gdt_descriptor gdt_descriptor_t;

struct gdt_entry
{
	short limit_low;
	short base_low;
	char base_middle;
	char access;
	char granularity;
	char base_high;
} __attribute__((packed));

typedef struct gdt_entry gdt_entry_t;

// Task State Segment
struct tss_entry
{
	uint prev_tss;   // The previous TSS - if we used hardware task switching this would form a linked list.
	uint esp0;       // The stack pointer to load when we change to kernel mode.
	uint ss0;        // The stack segment to load when we change to kernel mode.
	uint esp1;       // Unused...
	uint ss1;
	uint esp2;
	uint ss2;
	uint cr3;
	uint eip;
	uint eflags;
	uint eax;
	uint ecx;
	uint edx;
	uint ebx;
	uint esp;
	uint ebp;
	uint esi;
	uint edi;
	uint es;         // The value to load into ES when we change to kernel mode.
	uint cs;         // The value to load into CS when we change to kernel mode.
	uint ss;         // The value to load into SS when we change to kernel mode.
	uint ds;         // The value to load into DS when we change to kernel mode.
	uint fs;         // The value to load into FS when we change to kernel mode.
	uint gs;         // The value to load into GS when we change to kernel mode.
	uint ldt;        // Unused...
	unsigned short trap;
	unsigned short iomap_base;
} __attribute__((packed));

typedef struct tss_entry tss_entry_t;

class GDT
{
private:
	static gdt_entry_t gdt[GDT_ENTRIES];

	static gdt_descriptor_t gdt_descriptor;

	static void setup_tss(uint gdt_entry, uint ss0, uint esp0);

	/**
	 * Write a GDT entry
	 *
	 * @param gdt GDT
	 * @param num Entry number
	 * @param base Base address
	 * @param limit Segment limit
	 * @param access Access rights
	 * @param granularity Granularity flags
	 */
	static void
	set_entry(uint num, uint base, uint limit, char access, char granularity);

	/**
	 * Load the GDT
	 *
	 * @param gdt GDT descriptor
	 */
	static void load_asm();

	/**
	 * Load the TSS
	 *
	 * @warning This function must be called only after the GDT has been loaded with a TSS entry
	 */
	static void load_tss_asm();

public:

	/**
	 * Initialize the GDT and TSS
	 *
	 * @param gdt_descriptor GDT descriptor
	 * @param gdt GDT
	 */
	static void init();

	/**
	 * Define kernel stack location for handling interrupts / traps
	 *
	 * @param esp0 Stack pointer
	 */
	static void set_tss_kernel_stack(uint esp0);
};

#endif /* INCLUDE_OS_GDT_H */