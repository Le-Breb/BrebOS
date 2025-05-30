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

	static void wget(const cpu_state_t* cpu_state);

	static void wait_pid(Process* p);

	static void getenv(Process* p);

	/**
	 * Displays an image and make the process sleep for a bit - This is ugly, and I am aware of it
	 */
	__attribute__((no_instrument_function))
	static void feh(Process *p);

	/**
	 * Gets the screen dimensions
	 *
	 * Returns:
	 * EAX = screen width
	 * EDI = screen height
	 */
	static void get_screen_dimensions(Process* p);

	/**
	 * Loads a file into memory
	 * EDI = path
	 *
	 * Returns:
	 * EAX = pointer to file in memory, null if error
	 * EDI = size of file
	 */
	static void load_file(Process* p);

	static void write(Process* p);

	static int open(Process* p);

	static int read(Process* p);

	static int close(Process* p);
public:
	/**
	 * Handles a syscall
	 *
	 * @param cpu_state CPU state
	 * @param stack_state Stack state
	 * */
	[[noreturn]]

	static void dispatcher(const cpu_state_t* cpu_state, const stack_state_t* stack_state);
};

#endif //INCLUDE_SYSCALLS_H
