#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#include "interrupts.h"
#include "../processes/process.h"


class Syscall
{
	/**
	 * Starts a GRUB module as a child of current process
	 * @param p CPU state
	 * @param stack_state Stack state
	 */
	static void start_process(Process* p);

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
	 * @param ret_val
	 */
	[[noreturn]] static void terminate_process(Process* p, int ret_val);

	/**
	 * Tries to allocate dynamic memory for a given process
	 * @param p calling process
	 */
	static void malloc(Process* p);

	static void calloc(Process* p);

	/**
	 * Frees dynamic memory of a given process
	 * @param p calling process
	 */
	static void free(Process* p);

	static void realloc(Process* p);

	static void get_key();

	static void dynlk(const cpu_state_t* cpu_state);

	static void mkdir(cpu_state_t* cpu_state);

	static void touch(cpu_state_t* cpu_state);

	static void ls(cpu_state_t* cpu_state);

	static void dns(const cpu_state_t* cpu_state);

	static void cat(cpu_state_t* cpu_state);

	static void wget(const cpu_state_t* cpu_state);

	static void wait_pid(Process* p);

	static void getenv(Process* p);

	static void fork(Process *p);
public:
	/**
	 * Handles a syscall
	 *
	 * @param cpu_state CPU state
	 * @param stack_state Stack state
	 * */
	[[noreturn]]

	static void dispatcher(cpu_state_t* cpu_state, const stack_state_t* stack_state);
};

#endif //INCLUDE_SYSCALLS_H
