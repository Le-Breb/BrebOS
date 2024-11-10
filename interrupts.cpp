#include "interrupts.h"
#include "keyboard.h"
#include "clib/stdio.h"
#include "scheduler.h"
#include "syscalls.h"
#include "PIT.h"
#include "PIC.h"

/**
 * Changes current pdt
 *
 * @param pdt_phys_addr New PDT physical address
 * */
extern "C" void change_pdt_asm_(unsigned int pdt_phys_addr);

/**
 * Generic interrupt handler that calls the appropriate interrupt dispatcher
 *
 * @param kesp kernel ESP - Only meaningful when a syscall dispatcher has been interrupted, cause then CPU does not push ESP
 * nor ss but only eip cs and eflags. In that case, we are still using the syscall dispatcher stack, and kesp tells what ESP
 * was before the syscall dispatcher was interrupted
 * @param cpu_state CPU state
 * @param interrupt The interrupt number
 * @param stack_state Stack state
 */
extern "C" void
interrupt_handler(unsigned int kesp, cpu_state_t cpu_state, unsigned int interrupt, struct stack_state stack_state);

extern "C" void enable_interrupts_asm_(void);

extern "C" void disable_interrupts_asm_(void);

extern "C" [[noreturn]] void resume_user_process_asm_(cpu_state_t cpu_state, struct stack_state stack_state);

extern "C" [[noreturn]] void resume_syscall_handler_asm_(cpu_state_t cpu_state, unsigned int iret_esp);

void Interrupts::page_fault_handler(struct stack_state* stack_state)
{
	unsigned int addr; // Address of the fault
	__asm__ volatile("mov %%cr2, %0" : "=r"(addr)); // Get addr from CR2

	unsigned int err = stack_state->error_code;

	printf_error("Page fault at address 0x%x", addr);
	printf("Present: %s\n", (err & 1 ? "True" : "False"));
	printf("Write: %s\n", err & 2 ? "True" : "False");
	printf("User mode: %s\n", err & 4 ? "True" : "False");
	printf("Reserved: %s\n", err & 8 ? "True" : "False");
	printf("Instruction fetch: %s\n", err & 16 ? "True" : "False");
	printf("Protection key violation: %s\n", err & 32 ? "True" : "False");
	printf("Shadow stack: %s\n", err & 64 ? "True" : "False");
	printf("SGX: %s\n", err & 32768 ? "True" : "False");

	Scheduler::get_running_process()->terminate();
}

void Interrupts::gpf_handler(struct stack_state* stack_state)
{
	printf_error("General protection fault");
	printf("Segment selector: %x\n", stack_state->error_code);

	Scheduler::get_running_process()->terminate();
}

[[noreturn]] void Interrupts::interrupt_timer(unsigned int kesp, cpu_state_t* cpu_state, stack_state_t* stack_state)
{
	// Update time
	PIT::ticks++;


	Process* p = Scheduler::get_running_process();

	if (p)
	{
		// Did we interrupt a syscall handler ?
		unsigned int syscall_interrupted = stack_state->cs == 0x08;
		if (syscall_interrupted)
			p->set_flag(P_SYSCALL_INTERRUPTED);

		// Don't do anything if the process has been terminated
		if (!(p->is_terminated()))
		{
			// Update syscall handler state
			if (syscall_interrupted)
			{
				p->k_cpu_state = *cpu_state;
				p->k_stack_state = *stack_state;

				// Update ESP and SS manually because the CPU did not push them as no privilege level change occurred
				p->k_stack_state.esp = kesp;
				__asm__ volatile("mov %%ss, %0" : "=r"(p->k_stack_state.ss));
			}
			else // Update PCB
			{
				p->cpu_state = *cpu_state;
				p->stack_state = *stack_state;
			}
		}
	}


	PIC::acknowledge(0x20);

	Scheduler::schedule();
}

extern "C" void
interrupt_handler(unsigned int kesp, cpu_state_t cpu_state, unsigned int interrupt, struct stack_state stack_state)
{
	switch (interrupt)
	{
		case 0x20:
			Interrupts::interrupt_timer(kesp, &cpu_state, &stack_state);
		case 0x21:
			Keyboard::interrupt_handler();
			break;
		case 0x0e:
			Interrupts::page_fault_handler(&stack_state);
			break;
		case 0xD:
			Interrupts::gpf_handler(&stack_state);
			break;
		case 0x80:
			Syscall::dispatcher(&cpu_state, &stack_state);
		default:
			printf_error("Received unknown interrupt: %u", interrupt);
			break;
	}

	PIC::acknowledge(interrupt);

	// Handle the case where no process was running when the interrupt occurred. This typically happens when the CPU is
	// halted because all processes are waiting.
	// We do not want to return from the interrupt handler as we have nowhere to return to, so we trigger timer interrupt
	// which will run a process if one is ready once schedule is executed, or halt if all processes are waiting,
	// or shutdown if there is no more active processes
	if (!Scheduler::get_running_process())
		__asm__ volatile("int $0x20");
}

void Interrupts::enable_asm()
{
	enable_interrupts_asm_();
}

void Interrupts::disable_asm()
{
	disable_interrupts_asm_();
}

[[noreturn]] void Interrupts::resume_user_process_asm(cpu_state_t cpu_state, struct stack_state stack_state)
{
	resume_user_process_asm_(cpu_state, stack_state);
}

void Interrupts::resume_syscall_handler_asm(cpu_state_t cpu_state, unsigned int iret_esp)
{
	resume_syscall_handler_asm_(cpu_state, iret_esp);
}

void Interrupts::change_pdt_asm(unsigned int pdt_phys_addr)
{
	change_pdt_asm_(pdt_phys_addr);
}
