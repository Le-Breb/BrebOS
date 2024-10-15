#ifndef INCLUDE_INTERRUPTS_H
#define INCLUDE_INTERRUPTS_H

#include "io.h"

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

class Interrupts
{
public:
	/**
	 * Handles a page fault
	 *
	 * @param cpu_state CPU state
	 * @param stack_state Stack state
	 */
	static void page_fault_handler(struct stack_state* stack_state);

	/**
	 * Handles a GPF
	 *
	 * @param cpu_state CPU state
	 * @param stack_state Stack state
	 * */
	static void gpf_handler(struct stack_state* stack_state);

	/**
	 * Handler for preemption timer
	 * @param kesp Kernel ESP (see details in interrupt_handlers declaration)
	 * @param cpu_state CPU state
	 * @param stack_state stack state
	 */
	[[noreturn]] static void interrupt_timer(unsigned int kesp, cpu_state_t* cpu_state, stack_state_t* stack_state);

	/**
	 * Enables interrupts
	 */
	static void enable_asm();

	/**
	 * Disables interrupts
	 */
	static void disable_asm();

	/**
	* Exits from a syscall and resume user program
	*
	* @param cpu_state process CPU state
	* @param stack_state process stack state
	*/
	[[noreturn]] static void resume_user_process_asm(cpu_state_t cpu_state, struct stack_state stack_state);

	/**
	 * Exits from an interrupt and a resume an interrupted syscall dispatcher
	 * @param cpu_state syscall dispatcher CPU state
	 * @param iret_esp Value to set ESP to before executing IRET.
	 * IRET expects to find eip cs and eflags on the stack, which are pushed on the interrupted system dispatcher' stack.
	 * iret_esp should then be whatever ESP was when the syscall dispatcher was interrupted minus 12 = 3 * sizeof(int)
	 */
	[[noreturn]] static void resume_syscall_handler_asm(cpu_state_t cpu_state, unsigned int iret_esp);

	static void change_pdt_asm(unsigned int pdt_phys_addr);
};

#endif /* INCLUDE_INTERRUPTS_H */