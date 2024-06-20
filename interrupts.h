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

#define CLOCK_TICK_MS 10
#define TICKS_PER_SEC (1000 / CLOCK_TICK_MS)

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

typedef struct cpu_state cpu_state_t;

struct stack_state
{
	unsigned int error_code;
	unsigned int eip;
	unsigned int cs;
	unsigned int eflags;
	unsigned int esp;
	unsigned int ss;
} __attribute__((packed));

typedef struct stack_state stack_state_t;

#define KBD_DATA_PORT   0x60

/**
 * Initializes the IDT
 *
 * @param idt_descriptor IDT descriptor
 * @param idt IDT table
 */
void idt_init(idt_descriptor_t* idt_descriptor, idt_entry_t* idt);

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
void load_idt(idt_descriptor_t* idt_ptr);

/**
 * Generic interrupt handler that calls the appropriate interrupt handler
 *
 * @param kesp kernel ESP - Only meaningful when a syscall handler has been interrupted, cause then CPU does not push ESP
 * nor ss but only eip cs and eflags. In that case, we are still using the syscall handler stack, and kesp tells what ESP
 * was before the syscall handler was interrupted
 * @param cpu_state CPU state
 * @param interrupt The interrupt number
 * @param stack_state Stack state
 */
void
interrupt_handler(unsigned int kesp, cpu_state_t cpu_state, unsigned int interrupt, struct stack_state stack_state);

/**
 * Remaps the PIC interrupts
 *
 * @param offset1 The offset for the first PIC
 * @param offset2 The offset for the second PIC
 */
void pic_remap(int offset1, int offset2);

/**
 *  Reads a scan code from the keyboard
 *
 *  @return The scan code (NOT an ASCII character!)
 */
unsigned char read_scan_code(void);

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
void pic_init();

/**
 * Set up the PIT
 */
void pit_init();

/**
 * Exits from a syscall and resume user program
 *
 * @param cpu_state process CPU state
 * @param stack_state process stack state
 */
_Noreturn void resume_user_process_asm(cpu_state_t cpu_state, struct stack_state stack_state);

/**
 * Exits from an interrupt and a resume an interrupted syscall handler
 * @param cpu_state syscall handler CPU state
 * @param iret_esp Value to set ESP to before executing IRET.
 * IRET expects to find eip cs and eflags on the stack, which are pushed on the interrupted system handler' stack.
 * iret_esp should then be whatever ESP was when the syscall handler was interrupted minus 12 = 3 * sizeof(int)
 */
_Noreturn extern void resume_syscall_handler_asm(cpu_state_t cpu_state, unsigned int iret_esp);

void enable_preemptive_scheduling();

void disable_preemptive_scheduling();

unsigned int get_tick();

#endif /* INCLUDE_INTERRUPTS_H */