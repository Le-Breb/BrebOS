#ifndef CUSTOM_OS_SCHEDULER_H
#define CUSTOM_OS_SCHEDULER_H

#include "process.h"

// Maximum concurrent processes. Limit defined by size of pid_pool
#define MAX_PROCESSES (sizeof(uint) * 8)
#define MAX_THREADS (sizeof(uint) * 8)

typedef uint pid_t;
typedef uint tid_t;

class Process;
class Thread;

#define RESET_QUANTUM(t) (t->quantum = t->get_process()->priority * CLOCK_TICK_MS)

typedef struct
{
	pid_t arr[MAX_THREADS];
	uint start, count;
} ready_queue_t;

class Scheduler
{
private:
	// Bitmap of available PID. Ith LSB indicates PID state ; 1 = used, 0 = free.
	// Thus, 32 = sizeof(uint) PIDs are available, allowing up to 32 processes to run concurrently
	static uint pid_pool;
	static uint tid_pool;

	static pid_t running_thread;
	static ready_queue_t ready_queue;
	static ready_queue_t waiting_queue;
	static Process* processes[MAX_PROCESSES];
	static Thread* threads[MAX_THREADS];

	/**
	 * Round-robin scheduler
	 * @return next thread to run, NULL if there is no thread to run
	 */
	static Thread* get_next_thread();

	static pid_t get_free_pid();

	static pid_t get_free_tid();

	static void release_pid(pid_t pid);

	static void release_tid(tid_t tid);

	/**
	 * Create and st ready a process that will perform kernel initialization
	 * This process is used to execute parts of the initialization that rely on
	 * PIT::tick
	 */
	static void create_kernel_init_process();

	static uint get_free_spot(uint& values, uint max_value);

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

	static Thread* get_running_thread();

	static Thread** get_threads();

	static Process* get_running_process();

	static void wake_up_key_waiting_processes(char key);

	static void set_thread_ready(Thread* p);

	static void free_terminated_process(Process& p);

	static void set_first_ready_process_asleep_waiting_key_press();

	static void relinquish_first_ready_process();



	/**
	 * Stop kernel initialization process, leaving only potential user programs
	 * in the waiting queue
	 */
	static void stop_kernel_init_process();

	[[nodiscard]] static tid_t add_thread(Process* process);
};


#endif //CUSTOM_OS_SCHEDULER_H
