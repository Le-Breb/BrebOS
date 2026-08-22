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

	struct proc_waiting_for_read
	{
		pid_t pid;
		int fd;

		bool operator==(const proc_waiting_for_read& other) const {return other.pid == pid && fd == other.fd; }
	};

	static pid_t running_process;
	static queue<pid_t, MAX_PROCESSES>* ready_queue;
	static queue<pid_t, MAX_PROCESSES>* waiting_queue;
	static list<proc_waiting_for_read>* processes_waiting_for_read;
	static Process* processes[MAX_PROCESSES];
	static MinHeap<asleep_process>* sleeping_processes;
	static list<Process*>* exec_processes_to_free;
	static list<Process*>* processes_to_free;

	/**
	 * Round-robin scheduler
	 * @return next process to run, NULL if there is no process to run
	 */
	static Process* get_next_process();

	static void release_pid(pid_t pid);

	static void check_for_processes_to_wake_up();

	static void* stack_switch_stack_top;

	static void* idle_stack_top;

	static Process* load_process(const char* path, pid_t pid, pid_t ppid, int argc, const char** argv, const char** envp);

	static void wake_up_process_parent(pid_t process_pid);

	static bool signal_handling(Process* p);

	static void reparent_process_to_init(Process* p);

	static void delete_exec_processes();

	static void gc_processes();

	static void resume_process_(Process* p);

public:
	static pid_t init_pid;
	// When true, interrupt timer does not call schedule and simply resume the current process
	static bool preemption_lock;
	// Allows exiting a preemption locked section (eg portion of code where preemption lock is true)
	// In those critical sections, we cannot release the lock then call TRIGGER_TIMER_INTERRUPT, as the critical section
	// may then be reentered. This may sound harmless as the critical section apparently reached its end since it tries
	// to exit. However, this is actually an issue upon process termination. An example is when a signal that causes the
	// termination of the current process. Preemption shall then be disabled, to prevent any code to run on the
	// terminated process and ensure schedule is called, properly handling that scenario.
	// This bool then indicates interrupt_timer that preemption was locked to ensure schedule is called, and that is
	// interrupt_timer responsibility to unset critical_section_preempt_exir
	static bool critical_section_preempt_exit;

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
	 * @param envp
	 * @return new process' PID, -1 on error
	 */
	static int exec(const char* path, pid_t ppid, int argc, const char** argv, const char** envp);

	static void init();

	static void shutdown();

	static pid_t get_running_process_pid();

	static Process* get_running_process();

	static void wake_up_key_waiting_processes(char key);

	static void set_process_ready(Process* p);

	static void free_process(const Process& p);

	static void on_process_terminated(Process& p);

	static void set_first_ready_process_asleep_waiting_key_press();

	static void set_first_ready_process_asleep_waiting_process();

	static void set_first_ready_process_asleep_waiting_read();

	static void relinquish_first_ready_process();

	/**
	 * Stop kernel initialization process, leaving only potential user programs
	 * in the waiting queue
	 */
	static void stop_kernel_init_process();

	static int register_process_wait(pid_t waiting_process, pid_t waited_for_process, bool no_hang, bool& return_now);

	static void set_process_asleep(Process* p, uint duration);

	/**
	 * Starts a kernel process
	 * @param eip address of the function to call
	 */
	static void start_kernel_process(void* eip);

	static pid_t get_free_pid();

	static bool execve(Process* p, const char* path, int argc, const char** argv, const char** envp);

	[[nodiscard]]
	static Process* get_process(pid_t pid);

	[[noreturn]]
	static void resume_user_process(Process* p);

	[[noreturn]]
	static void resume_syscall_handler(Process* p);

	static void do_read_wait(pid_t process_pid, int fd);

	static void wake_up_read_waiting_processes(int write_fd, int read_fd);

	[[noreturn]]
	static void resume_process(Process* p);

	[[nodiscard]]
	static Memory::page_table_t* get_current_page_tables();
};


#endif //CUSTOM_OS_SCHEDULER_H
