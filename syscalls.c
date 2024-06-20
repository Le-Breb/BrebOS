#include "syscalls.h"
#include "interrupts.h"
#include "process.h"
#include "clib/stdio.h"
#include "system.h"

/**
 * Starts a GRUB module as a child of current process
 * @param cpu_state CPU state
 * @param stack_state Stack state
 */
void syscall_start_process(cpu_state_t* cpu_state);

/**
 * Returns current process' PID  */
void syscall_get_pid();

/**
 * Prints a formatted string
 * @param cpu_state CPU state. EBX format string, ECX = pointer to printf arguments (on user stack)
 */
void syscall_printf(cpu_state_t* cpu_state);

void syscall_start_process(cpu_state_t* cpu_state)
{
	// Load child process and set it ready
	start_module(cpu_state->ebx, get_running_process_pid());
}

void syscall_printf(cpu_state_t* cpu_state)
{
	printf_syscall((char*) cpu_state->ebx, (char*) cpu_state->ecx);
}

void syscall_get_pid()
{
	process* running_process = get_running_process();

	running_process->cpu_state.eax = running_process->pid; // Return PID
}

void syscall_get_key()
{
	process* p = get_running_process();
	p->flags |= P_WAITING_KEY;

	__asm__ volatile("int $0x20");
}

_Noreturn void syscall_handler(cpu_state_t* cpu_state, struct stack_state* stack_state)
{
	process* p = get_running_process();

	// Update PCB
	p->cpu_state = *cpu_state;
	p->stack_state = *stack_state;

	switch (cpu_state->eax)
	{
		case 1:
			terminate_process(p);

			// We definitely do not want to continue as next step is to resume the process we just terminated !
			// Instead, we manually raise a timer interrupt which we schedule another process
			// (The terminated one as its terminated flag set, so it won't be selected by the scheduler)
			__asm__ volatile("int $0x20");

			// We will never resume the code here
			__builtin_unreachable();
		case 2:
			syscall_printf(cpu_state);
			break;
		case 3:
			disable_preemptive_scheduling();
			syscall_start_process(cpu_state);
			enable_preemptive_scheduling();
			break;
		case 4:
			syscall_get_key();
			break;
		case 5:
			syscall_get_pid();
			break;
		case 6:
			shutdown();
			break;
		default:
			printf_error("Received unknown syscall id: %u", cpu_state->eax);
			break;
	}

	resume_user_process_asm(p->cpu_state, p->stack_state);
}