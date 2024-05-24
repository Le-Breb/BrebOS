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
#define PIC2_END_INTERRUPT   PIC2_START_INTERRUPT + 7

#define PIC_ACK     0x20

#define INTGATE  0x8E00
#define NUM_INTERRUPTS 0xFF

struct idt
{
	unsigned short size;
	void* address;
} __attribute__ ((packed));

struct idt_entry
{
	unsigned short offset0_15;
	unsigned short select;
	unsigned short type;
	unsigned short offset16_31;
} __attribute__ ((packed));

/** pic_acknowledge:
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
	unsigned int esp;
} __attribute__((packed));

struct stack_state
{
	unsigned int error_code;
	unsigned int eip;
	unsigned int cs;
	unsigned int eflags;
} __attribute__((packed));

#define KBD_DATA_PORT   0x60

/** read_scan_code:
 *  Reads a scan code from the keyboard
 *
 *  @return The scan code (NOT an ASCII character!)
 */
unsigned char read_scan_code(void);

void idt_init(struct idt* idt_ptr, struct idt_entry* idt);

void idt_set_entry(struct idt_entry* idt, int num, unsigned int base, unsigned short select, unsigned short type);

void load_idt(struct idt* idt_ptr);

void interrupt_handler([[maybe_unused]] struct cpu_state cpu_state, unsigned int interrupt, [[maybe_unused]] struct stack_state stack_state);

void pic_remap(int offset1, int offset2);

void enable_interrupts(void);

void disable_interrupts(void);

void setup_pic();

#endif /* INCLUDE_INTERRUPTS_H */