#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#include "interrupts.h"
#include "process.h"


class Syscall
{
	/**
	 * Starts a GRUB module as a child of current process
	 * @param cpu_state CPU state
	 * @param stack_state Stack state
	 */
	static void start_process(cpu_state_t* cpu_state);

	/**
	 * Returns current process' PID  */
	static void get_pid();

	/**
	 * Prints a formatted string
	 * @param cpu_state CPU state. EBX format string, ECX = pointer to printf arguments (on user stack)
	 */
	static void printf(cpu_state_t* cpu_state);

	/**
	 * Terminates a process
	 * @param p process to terminate
	 */
	[[noreturn]] static void terminate_process(Process* p);

	/**
	 * Tries to allocate dynamic memory for a given process
	 * @param p calling process
	 * @param cpu_state process CPU state
	 */
	static void malloc(Process* p, cpu_state_t* cpu_state);

	/**
	 * Frees dynamic memory of a given process
	 * @param p calling process
	 * @param cpu_state process CPU state
	 */
	static void free(Process* p, cpu_state_t* cpu_state);

	static void get_key();

	static void dynlk(cpu_state_t* cpu_state);

	static void mkdir(cpu_state_t* cpu_state);

	static void touch(cpu_state_t* cpu_state);

	static void ls(cpu_state_t* cpu_state);

public:
	/**
	 * Handles a syscall
	 *
	 * @param cpu_state CPU state
	 * @param stack_state Stack state
	 * */
	[[noreturn]]

	static void dispatcher(cpu_state_t* cpu_state, struct stack_state* stack_state);
};

#endif //INCLUDE_SYSCALLS_H
