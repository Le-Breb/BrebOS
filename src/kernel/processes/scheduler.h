#ifndef CUSTOM_OS_SCHEDULER_H
#define CUSTOM_OS_SCHEDULER_H

#include "process.h"
#include "../utils/min_heap.h"
#include "../utils/queue.h"

// Maximum concurrent processes. Limit defined by size of pid_pool
#define MAX_PROCESSES (sizeof(uint) * 8)

#define RESET_QUANTUM(p) (p->quantum = p->priority * CLOCK_TICK_MS)

class Scheduler
{
private:
	// Bitmap of available PID. Ith LSB indicates PID state ; 1 = used, 0 = free.
	// Thus, 32 = sizeof(uint) PIDs are available, allowing up to 32 processes to run concurrently
	static uint pid_pool;

	// Represents a sleeping process. Comparison is done using  end tick in order to sort them in a min heap,
	// so that we only need to check the first process in the heap to know if there are processes to wake up
	struct asleep_process
	{
		Process* process;
		uint end_tick;
		bool operator<(const asleep_process& other) const
		{
			return end_tick < other.end_tick;
		}
		bool operator>(const asleep_process& other) const
		{
			return end_tick > other.end_tick;
		}
	};

	static pid_t running_process;
	static queue<pid_t, MAX_PROCESSES>* ready_queue;
	static queue<pid_t, MAX_PROCESSES>* waiting_queue;
	static list<pid_t> process_waiting_list[MAX_PROCESSES];
	static Process* processes[MAX_PROCESSES];
	static MinHeap<asleep_process>* sleeping_processes;

	/**
	 * Round-robin scheduler
	 * @return next process to run, NULL if there is no process to run
	 */
	static Process* get_next_process();

	static pid_t get_free_pid();

	static void release_pid(pid_t pid);

	static void check_for_processes_to_wake_up();

public:
	/**
	 * Create the process that will be used for global kernel memory mappings, and which will handle the end of kernel
	 * initialization, when PIT is needed (likely for parts that need to call sleep).
	 *
	 * @note See Memory::create_kernel_process for additional information
	 * @param process_host_mem memory region large enough the store a process
	 * @param lowest_free_pe lowest free page entry
	 * @param kernel_process where to write the constructed process
	 */
	static void create_kernel_init_process(void* process_host_mem, uint lowest_free_pe, Process** kernel_process);

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

	static void shutdown();

	static pid_t get_running_process_pid();

	static Process* get_running_process();

	static void wake_up_key_waiting_processes(char key);

	static void set_process_ready(Process* p);

	static void free_terminated_process(Process& p);

	static void set_first_ready_process_asleep_waiting_key_press();

	static void set_first_ready_process_asleep_waiting_process();

	static void relinquish_first_ready_process();

	/**
	 * Stop kernel initialization process, leaving only potential user programs
	 * in the waiting queue
	 */
	static void stop_kernel_init_process();

	static bool register_process_wait(pid_t waiting_process, pid_t waiting_for_process);

	static pid_t fork(Process* p);

	static void set_process_asleep(Process* p, uint duration);

	/**
	 * Starts a kernel process
	 * @param eip address of the function to call
	 */
	static void start_kernel_process(void* eip);
};


#endif //CUSTOM_OS_SCHEDULER_H
