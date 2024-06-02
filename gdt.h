#ifndef INCLUDE_K_H
#define INCLUDE_K_H

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
	unsigned int prev_tss;   // The previous TSS - if we used hardware task switching this would form a linked list.
	unsigned int esp0;       // The stack pointer to load when we change to kernel mode.
	unsigned int ss0;        // The stack segment to load when we change to kernel mode.
	unsigned int esp1;       // Unused...
	unsigned int ss1;
	unsigned int esp2;
	unsigned int ss2;
	unsigned int cr3;
	unsigned int eip;
	unsigned int eflags;
	unsigned int eax;
	unsigned int ecx;
	unsigned int edx;
	unsigned int ebx;
	unsigned int esp;
	unsigned int ebp;
	unsigned int esi;
	unsigned int edi;
	unsigned int es;         // The value to load into ES when we change to kernel mode.
	unsigned int cs;         // The value to load into CS when we change to kernel mode.
	unsigned int ss;         // The value to load into SS when we change to kernel mode.
	unsigned int ds;         // The value to load into DS when we change to kernel mode.
	unsigned int fs;         // The value to load into FS when we change to kernel mode.
	unsigned int gs;         // The value to load into GS when we change to kernel mode.
	unsigned int ldt;        // Unused...
	unsigned short trap;
	unsigned short iomap_base;
} __attribute__((packed));

typedef struct tss_entry tss_entry_t;

/**
 * Load the GDT
 *
 * @param gdt GDT descriptor
 */
void load_gdt(struct gdt_descriptor* gdt); // Extern

/**
 * Load the TSS
 *
 * @warning This function must be called only after the GDT has been loaded with a TSS entry
 */
void load_tss(); // Extern

/**
 * Initialize the GDT and TSS
 *
 * @param gdt_descriptor GDT descriptor
 * @param gdt GDT
 */
void gdt_init(gdt_descriptor_t* gdt_descriptor, gdt_entry_t* gdt);

/**
 * Define kernel stack location for handling interrupts / traps
 *
 * @param esp0 Stack pointer
 */
void set_tss_kernel_stack(unsigned int esp0);

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
void gdt_set_entry(struct gdt_entry* gdt, unsigned int num, unsigned int base, unsigned int limit, char access,
				   char granularity);

#endif /* INCLUDE_K_H */