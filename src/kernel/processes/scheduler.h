#ifndef CUSTOM_OS_SCHEDULER_H
#define CUSTOM_OS_SCHEDULER_H

#include "process.h"

// Maximum concurrent processes. Limit defined by size of pid_pool
#define MAX_PROCESSES (sizeof(uint) * 8)

#define RESET_QUANTUM(p) (p->quantum = p->priority * CLOCK_TICK_MS)

typedef struct
{
	pid_t arr[MAX_PROCESSES];
	uint start, count;
} ready_queue_t;

class Scheduler
{
private:
	// Bitmap of available PID. Ith LSB indicates PID state ; 1 = used, 0 = free.
	// Thus, 32 = sizeof(uint) PIDs are available, allowing up to 32 processes to run concurrently
	static uint pid_pool;

	static pid_t running_process;
	static ready_queue_t ready_queue;
	static ready_queue_t waiting_queue;
	static Process* processes[MAX_PROCESSES];

	/**
	 * Round-robin scheduler
	 * @return next process to run, NULL if there is no process to run
	 */
	static Process* get_next_process();

	static pid_t get_free_pid();

	static void release_pid(pid_t pid);

	/**
	 * Create and st ready a process that will perform kernel initialization
	 * This process is used to execute parts of the initialization that rely on
	 * PIT::tick
	 */
	static void create_kernel_init_process();

public:
	/**
	 * Executes next process in the ready queue
	 */
	[[noreturn]] static void schedule();

	static void start_module(uint module, pid_t ppid, int argc, const char** argv);

	/**
	 * 
	 * @param path path to ELF
	 * @param ppid parent PID
	 * @param argc number of program arguments
	 * @param argv program arguments
	 * @return new process' PID, -1 on error
	 */
	static int exec(const char* path, pid_t ppid, int argc, const char** argv);

	static void init();

	static pid_t get_running_process_pid();

	static Process* get_running_process();

	static void wake_up_key_waiting_processes(char key);

	static void set_process_ready(Process* p);

	static void free_terminated_process(Process& p);

	static void set_first_ready_process_asleep_waiting_key_press();

	static void relinquish_first_ready_process();

	/**
	 * Stop kernel initialization process, leaving only potential user programs
	 * in the waiting queue
	 */
	static void stop_kernel_init_process();
};


#endif //CUSTOM_OS_SCHEDULER_H
