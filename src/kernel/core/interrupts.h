#ifndef INCLUDE_INTERRUPTS_H
#define INCLUDE_INTERRUPTS_H

#include "IO.h"

#define TRIGGER_TIMER_INTERRUPT __asm__ volatile("int $0x20");

struct cpu_state
{
	uint eax;
	uint ebx;
	uint ecx;
	uint edx;
	uint esi;
	uint edi;
	uint ebp;
} __attribute__((packed));

typedef struct cpu_state cpu_state_t;

struct stack_state
{
	uint error_code;
	uint eip;
	uint cs;
	uint eflags;
	uint esp;
	uint ss;
} __attribute__((packed));

typedef struct stack_state stack_state_t;

class Interrupt_handler;

class Interrupts
{
public:
	static Interrupt_handler* handlers[256];
	/**
	 * Handles a page fault
	 *
	 * @param cpu_state CPU state
	 * @param stack_state Stack state
	 */
	static void page_fault_handler(const struct stack_state* stack_state);

	/**
	 * Handles a GPF
	 *
	 * @param cpu_state CPU state
	 * @param stack_state Stack state
	 * */
	static void gpf_handler(const struct stack_state* stack_state);

	/**
	 * Handler for preemption timer
	 * @param kesp Kernel ESP (see details in interrupt_handlers declaration)
	 * @param cpu_state CPU state
	 * @param stack_state stack state
	 */
	[[noreturn]] static void interrupt_timer(uint kesp, cpu_state_t* cpu_state, stack_state_t* stack_state);

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
	[[noreturn]] static void resume_user_process_asm(const cpu_state_t* cpu_state, const struct stack_state* stack_state);

	/**
	 * Exits from an interrupt and a resume an interrupted syscall
	 * @param cpu_state syscall CPU state
	 * @param stack_state syscall stack state
	 */
	[[noreturn]] static void resume_syscall_handler_asm(cpu_state_t* cpu_state, stack_state_t* stack_state);

	static void change_pdt_asm(uint pdt_phys_addr);

	static bool register_interrupt(uint interrupt_id, Interrupt_handler* handler);

	static void dynamic_interrupt_dispatcher(uint interrupt, cpu_state_t* cpu_state, stack_state_t* stack_state);

	static void debug_handler(const cpu_state_t* cpu_state, const stack_state_t* stack_state);
};

#endif /* INCLUDE_INTERRUPTS_H */
