#ifndef CUSTOM_OS_IDT_H
#define CUSTOM_OS_IDT_H

#include <kstddef.h>

#define INTGATE  0x8E00
#define SYSCALL  0xEF00
#define NUM_INTERRUPTS 0xFF

// Interrupt Descriptor Table
struct idt_descriptor
{
	unsigned short size;
	void* address;
} __attribute__ ((packed));

typedef struct idt_descriptor idt_descriptor_t;

struct idt_entry
{
	unsigned short offset0_15;
	unsigned short select;
	unsigned short type;
	unsigned short offset16_31;
} __attribute__ ((packed));

typedef struct idt_entry idt_entry_t;

class IDT
{
private:
	static idt_entry_t idt[256];

	static idt_descriptor_t idt_descriptor;

	/**
	 * Sets an entry in the IDT
	 *
	 * @param idt IDT table
	 * @param num The number of the interrupt
	 * @param base The base address of the interrupt dispatcher
	 * @param select The selector
	 * @param type The type of the interrupt
	 */
	static void set_entry(int num, uint base, unsigned short select, unsigned short type);

	/**
	 * Loads the IDT
	 *
	 * @param idt_ptr The IDT descriptor
	 */
	static void load_idt_asm();

public:
	/**
	 * Initializes the IDT
	 *
	 * @param idt_descriptor IDT descriptor
	 * @param idt IDT table
	 */
	static void init();
};


#endif //CUSTOM_OS_IDT_H
