#ifndef INCLUDE_INTERRUPTS_H
#define INCLUDE_INTERRUPTS_H

#include "io.h"

#define PIC1         0x20    // IO base address for master PIC
#define PIC2         0xA0    // IO base address for slave PIC
#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2 + 1)

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

/* The PIC interrupts have been remapped */
#define PIC1_START_INTERRUPT 0x20
#define PIC2_START_INTERRUPT 0x28
#define PIC2_END_INTERRUPT (PIC2_START_INTERRUPT + 7)

#define PIC_ACK     0x20

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

/**
 * pic_acknowledge:
 *  Acknowledges an interrupt from either PIC 1 or PIC 2.
 *
 *  @param num The number of the interrupt
 */
void pic_acknowledge(unsigned int interrupt);

struct cpu_state
{
	unsigned int eax;
	unsigned int ebx;
	unsigned int ecx;
	unsigned int edx;
	unsigned int esi;
	unsigned int edi;
	unsigned int ebp;
} __attribute__((packed));

struct stack_state
{
	unsigned int error_code;
	unsigned int eip;
	unsigned int cs;
	unsigned int eflags;
	unsigned int esp;
	unsigned int ss;
} __attribute__((packed));

#define KBD_DATA_PORT   0x60

/**
 *  Reads a scan code from the keyboard
 *
 *  @return The scan code (NOT an ASCII character!)
 */
unsigned char read_scan_code(void);

/**
 * Initializes the IDT
 *
 * @param idt_descriptor IDT descriptor
 * @param idt IDT table
 */
void idt_init(idt_descriptor_t * idt_descriptor, idt_entry_t * idt);

/**
 * Sets an entry in the IDT
 *
 * @param idt IDT table
 * @param num The number of the interrupt
 * @param base The base address of the interrupt handler
 * @param select The selector
 * @param type The type of the interrupt
 */
void idt_set_entry(idt_entry_t* idt, int num, unsigned int base, unsigned short select, unsigned short type);

/**
 * Loads the IDT
 *
 * @param idt_ptr The IDT descriptor
 */
void load_idt(idt_descriptor_t * idt_ptr);

/**
 * Generic interrupt handler that calls the appropriate interrupt handler
 *
 * @param cpu_state CPU state
 * @param interrupt The interrupt number
 * @param stack_state Stack state
 */
void interrupt_handler([[maybe_unused]] struct cpu_state cpu_state, unsigned int interrupt, [[maybe_unused]] struct stack_state stack_state);

/**
 * Remaps the PIC interrupts
 *
 * @param offset1 The offset for the first PIC
 * @param offset2 The offset for the second PIC
 */
void pic_remap(int offset1, int offset2);

/**
 * Enables interrupts
 */
void enable_interrupts(void);

/**
 * Disables interrupts
 */
void disable_interrupts(void);

/**
 * Sets up the PIC
 */
void setup_pic();

/** Switch to user mode. External assembly function
 *
 * @param pdt PDT physical address
 * @param eip Instruction pointer
 * @param cs Code segment
 * @param eflags Flags
 * @param esp Stack pointer
 * @param ss Stack segment
 */
void user_mode_jump_asm(unsigned int pdt, unsigned int eip, unsigned int cs, unsigned int eflags, unsigned int esp,
						unsigned int ss);

/** Exit from a syscall and resume user program
 *
 * @param cpu_state saved CPU state
 * @param stack_state saved stack state
 */
void syscall_exit_asm(struct cpu_state cpu_state, struct stack_state stack_state);

#endif /* INCLUDE_INTERRUPTS_H */