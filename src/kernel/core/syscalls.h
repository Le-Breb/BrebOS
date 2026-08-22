#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

#include "interrupts.h"
#include "../processes/process.h"
#include <signal.h>

class Syscall
{
	// Indicates what to do once a syscall has executed
	// ReturnToUser is trivial
	// Schedule requires a call to schedule, before returning to user
	// ScheduleExit requires a call to schedule and SHALL NOT even return
	enum class SyscallResult { ReturnToUser, Schedule, ScheduleExit};
	/**
	 * Returns current process' PID  */
	static SyscallResult get_pid(Process* p);

	/**
	 * Prints a formatted string
	 * @param cpu_state CPU state. EBX format string, ECX = pointer to printf arguments (on user stack)
	 */
	static SyscallResult printf(cpu_state_t* cpu_state);

	/**
	 * Terminates a process
	 * @param p process to terminate
	 * @param ret_val
	 */
	static SyscallResult terminate_process(Process* p, int ret_val);

	/**
	 * Tries to allocate dynamic memory for a given process
	 * @param p calling process
	 */
	static SyscallResult malloc(Process* p);

	static SyscallResult calloc(Process* p);

	/**
	 * Frees dynamic memory of a given process
	 * @param p calling process
	 */
	static SyscallResult free(Process* p);

	static SyscallResult realloc(Process* p);

	static SyscallResult get_key();

	static SyscallResult mkdir(cpu_state_t* cpu_state);

	static SyscallResult touch(cpu_state_t* cpu_state);

	static SyscallResult ls(cpu_state_t* cpu_state);

	static SyscallResult dns(const cpu_state_t* cpu_state);

	static SyscallResult wget(const cpu_state_t* cpu_state);

	static SyscallResult wait_pid(Process* p);

	static SyscallResult lseek(Process* p);

	/**
	 * Displays an image and make the process sleep for a bit - This is ugly, and I am aware of it
	 */
	static SyscallResult feh(Process *p);

	/**
	 * Gets the screen dimensions
	 *
	 * Returns:
	 * EAX = screen width
	 * EDI = screen height
	 */
	static SyscallResult get_screen_dimensions(Process* p);

	/**
	 * Loads a file into memory
	 * EDI = path
	 *
	 * Returns:
	 * EAX = pointer to file in memory, null if error
	 * EDI = size of file
	 */
	static SyscallResult load_file(Process* p);

	static SyscallResult write(Process* p);

	static SyscallResult open(Process* p);

	static SyscallResult read(Process* p);

	static SyscallResult close(Process* p);

	static SyscallResult stat(Process* p);

	static SyscallResult fstat(Process* p);

	static SyscallResult kill(Process* p);

	static SyscallResult signal(Process* p);

	static SyscallResult signal_return(Process* p);

	static SyscallResult fcntl(Process* p);

	static SyscallResult dup(Process* p);

	static SyscallResult dup2(Process* p);

	static SyscallResult pipe(Process* p);

	static SyscallResult getcwd(Process* p);

	static SyscallResult chdir(Process* p);

	static SyscallResult tcbset(Process* p);

	static SyscallResult isatty(Process* process);

	static SyscallResult sigaction(Process* p);

	static SyscallResult sigprocmask(Process* p);

	static SyscallResult mmap(Process* p);

	static SyscallResult mprotect(Process* p);

	static SyscallResult execve(Process* p);

	static SyscallResult sleep(Process* p);

	static SyscallResult opendir(Process* process);

	static SyscallResult getdents(Process* p);
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
