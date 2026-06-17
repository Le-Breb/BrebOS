#ifndef INCLUDE_PROCESS_H
#define INCLUDE_PROCESS_H

#include <signal.h>
#include "../core/memory.h"
#include "../core/interrupts.h"
#include "ELF.h"
#include "../utils/list.h"
#include "../file_management/VFS.h"
#include <kstddef.h>
#include <abi-bits/signal.h>

#include "stdarg.h"
#include "../utils/min_heap.h"
#include "../utils/Stack.h"

// Process is ready to be executed
#define P_READY 0
// Process is terminated but not freed
#define P_TERMINATED 1
// Process has been interrupted during a syscall
#define P_SYSCALL_INTERRUPTED 2
// Process is waiting for a key press
#define P_WAITING_KEY 4
// Process is waiting for another process to terminate
#define P_WAITING_PROCESS 8
// Process is terminated, but not freed for its parent to get info from it
#define P_ZOMBIE 16
// Process is sleeping
#define P_SLEEPING 32
// Process is about to be replaced because of an exec
#define P_EXEC 64
// Process is waiting for new data after calling read()
#define P_WAITING_READ 128

#define INIT_ERR_RET_VAL 127

// Can be increased up to <= sizeof(sigset_t) * 8.
// Do not forget to update @signal_default_action initialization accordingly if you increase that value
// Do not forget to update @sig_names too
#define HIGHEST_SIGNAL 12
#define MAX_CONCURRENT_SIGNAL_HANDLERS 10

#define SIGDISP_TERM 0
#define SIGDISP_IGN  1
#define SIGDISP_CORE 2
#define SIGDISP_STOP 3
#define SIGDISP_CONT 4
#define SIGDISP_HLDR 5

#define PROCESS_STACK_SIZE 0x80000 // 512 KiB
static_assert((PROCESS_STACK_SIZE % PAGE_SIZE) == 0, "PROCESS_STACK_SIZE must be a multiple of STACK_SIZE");
#define PROCESS_STACK_N_PAGES (PROCESS_STACK_SIZE / PAGE_SIZE)
#define PROCESS_SYSCALL_STACK_SIZE 0x2000
static_assert((PROCESS_SYSCALL_STACK_SIZE % PAGE_SIZE) == 0, "PROCESS_SYSCALL_STACK_SIZE must be a multiple of STACK_SIZE");
#define PROCESS_SYSCALL_STACK_N_PAGES (PROCESS_SYSCALL_STACK_SIZE / PAGE_SIZE)

class Scheduler;

class Process
{
	friend class Scheduler; // Scheduler managers processes, it needs complete access to do its stuff
	friend class ELFLoader; // ELFLoader creates processes, it acts like the constructor, it initializes most fields

	struct address_val_pair
	{
		void* address;
		int value;
	};

	struct file_descriptor
	{
		file_descriptor(int fd, int sys_fd, bool clo_exec = false)
			: fd(fd),
			  sys_fd(sys_fd),
			  clo_exec(clo_exec)
		{
			if (VFS::file_descriptors[sys_fd] == nullptr)
				irrecoverable_error("Trying to create a dangling file descriptor");
			VFS::file_descriptors[sys_fd]->rc++;
		}
		~file_descriptor()
		{
			if (VFS::file_descriptors[sys_fd] == nullptr)
				irrecoverable_error("Trying to delete a dangling file descriptor");
			VFS::file_descriptors[sys_fd]->rc--;
		}

		int fd = -1;
		int sys_fd = -1;
		bool clo_exec = false;


	};

	uint quantum, priority;

	uint num_pages; // Num pages over which the process code spans, including unmapped pages

	pid_t pid; // PID
	pid_t ppid; // Parent PID

	uint k_stack_top; // Top of syscall handlers' stack
	uint flags; // Process state

	int ret_status{};

	list<void*> allocations{};
	char* work_dir;

	Process* exec_replacement = nullptr; // Process which will replace this program after a call to execve

	bool is_pre_freed = false;

public:
	file_descriptor* file_descriptors[MAX_FD_PER_PROCESS]{};
	// Lowest free file descriptor, 0, 1 and 2 are reserved for stdin, stdout and stderr. They are not implemented yet.
	// but its for POSIX compatibility
	int lowest_free_fd = 3;
private:
	__sighandler signal_action[HIGHEST_SIGNAL + 1]{}; // Action for each signal
	static int signal_default_action[HIGHEST_SIGNAL + 1]; // Default action for each signal
	struct signal_ctx
	{
		cpu_state_t cpu_state;
		stack_state_t stack_state;
		sigset_t blocked_mask;
	};
	Stack<signal_ctx> signals_contexts{MAX_CONCURRENT_SIGNAL_HANDLERS};
	struct sigaction* oldact[HIGHEST_SIGNAL + 1]{};
	sigset_t pending_signals{};
	sigset_t* signal_top_level_block_mask = nullptr; // Ptr to signals_contexts[0].blocked_mask. Do not free

	void copy_page_to_other_process(const Process* other, uint page_id, uint mapping_page_id) const;

	void copy_page_to_other_process_shared(const Process* other, uint page_id) const;

public:
	uint lowest_free_pe;
	uint free_bytes = 0;

	char* bin_path = nullptr;
	cpu_state_t cpu_state{}; // Registers
	cpu_state_t k_cpu_state{}; // Syscall handler registers
	stack_state_t stack_state{}; // Execution context
	stack_state_t k_stack_state{}; // Syscall handler execution context

	bool is_waiting_for_any_child_to_terminate = false; // Tells whether the process is waiting for any child to terminate
	bool is_waited_by_parent = false; // Tells whether the parent specifically waited for this process to terminate
	list<pid_t> children{};
	list<address_val_pair> values_to_write{}; // list of values that need to be written in process address space

	list<Memory::allocation> mmap_allocations{};
	Memory::memory_header mem_base{.s = {&mem_base, 0}};
	Memory::memory_header* freep = &mem_base; // list of memory blocks allocated by the process
	void* tls_base = nullptr;

	// Those fields have to be first for alignment constraints
	// Process page tables. Process can use all virtual addresses below the kernel virtual location at pde 768
	Memory::page_table_t* page_tables;
	Memory::pdt_t* pdt; // process page directory table

	Elf32_Addr program_break;

private:
	/** Frees a terminated process */
	~Process();

	/** Frees the resources of the process (typically used for zombies to release most memory) **/
	void pre_free();

	Process(char* bin_path, uint num_pages, Memory::page_table_t* page_tables,
		Memory::pdt_t* pdt, stack_state_t* stack_state, uint priority, pid_t pid,
		pid_t ppid, Elf32_Addr k_stack_top);

	/**
	 * Loads a flat binary in memory
	 *
	 * @param module Grub module id
	 * @param module grub modules
	 * @return program's process, NULL if an error occurred
	 */
	//static Process* from_binary(GRUB_module* module);

	int get_free_fd();

	void release_fd(int fd);

public:
	/** Gets the process' PID */
	[[nodiscard]] pid_t get_pid() const;

	/**
	 * Terminates a process that exited normally
	 */
	void terminate_with_value(int ret_val);

	/**
	 * Terminates a process that exited because of a signal
	 */
	void terminate_with_signal(int ret_sig);

	/**
	 * Contiguous heap memory allocator
	 * @param n required memory quantity
	 * @return pointer to the beginning of allocated heap memory, NULL if an error occurred
	 */
	[[nodiscard]] void* malloc(uint n);

	[[nodiscard]] void* calloc(size_t nmemb, size_t size);

	[[nodiscard]] void* realloc(void* ptr, size_t size);

	/**
	 * Contiguous heap memory free
	 * @param ptr pointer to the beginning of allocated heap memory
	 */
	void free(void* ptr);

	/**
	 * Sets a flag
	 * @param flag flag to set
	 */
	void set_flag(uint flag);

	[[nodiscard]] const char* get_work_dir() const;

	/** Checks whether the program is terminated */
	[[nodiscard]] bool is_terminated() const;

	/** Checks whether the program is waiting for a key press */
	[[nodiscard]] bool is_waiting_key() const;

	/** Checks whether the program is waiting for another program to terminate */
	[[nodiscard]] bool is_waiting_program() const;

	/** Checks whether the program is waiting for new data after a call to read() **/
	[[nodiscard]] bool is_waiting_read() const;

	[[nodiscard]] bool is_sleeping() const;

	[[nodiscard]] bool exec_running() const;

	static void init();

	pid_t fork();

	/**
	 * Updates a page table entry.
	 * @param pte page id
	 * @param val new page entry value
	 * @param update_cache whether TLB cache should be updated after applying the update
	 */
	void update_pte(uint pte, uint val, bool update_cache) const;

	/**
	 * Transfer various data to proc, which is set as exec replacement
	 * @param proc process that whill replace this process
	 */
	void execve_transfer(Process* proc);

	void register_mmap_allocation(const Memory::allocation& allocation);

	bool deallocate(void* addr, Memory::allocation& alloc);

	void free_leaks();

	/**
	 * Opens a file
	 * @param pathname path of the file to open
	 * @param flags opening parameters
	 * @param mode mode bits used when a file is created
	 * @return file identifier on success, -2 if system-wide fd limit is reached, -3 if bad fd, -4 if process fd limit reached
	 */
	int open(const char* pathname, int flags, mode_t mode);

	/**
	 * Reads data from a file descriptor
	 * @param fd file descriptor to read from
	 * @param buf buffer to read data into
	 * @param count how many bytes to read
	 * @return number of bytes read on success, -2 if fd not open, -3 if not a regular file, -4 if IO error
	 */
	int read(int fd, void* buf, size_t count) const;

	int write(int fd, uint count, void* buf) const;

	int fstat(int fd, struct stat* statbuf) const;

	int stat(const char* pathname, struct stat* statbuf) const;

	/**
	 * Closes a file descriptor
	 * @param fd file descriptor to close
	 * @return 0 on success, -2 if fd not open
	 */
	int close(int fd);

	[[nodiscard]]
	int lseek(int fd, int offset, int whence) const;

	[[nodiscard]]
	int proc_to_sys_fd(int fd) const;

	int kill(int signal);

	__sighandler register_signal_handler(int signal, __sighandler handler);

	void signal_context_save(int signal);

	void signal_context_restore();

	int fcntl(int fd, int op, va_list arg) const;

	int dup(int oldfd);

	int dup2(int oldfd, int newfd);

	int pipe(int pipefd[2]);

	int chdir(const char* path);

	[[nodiscard]]
	int isatty(int fd) const;

	int sigaction(int signum, const struct sigaction* act, struct sigaction* old_act);

	int sigprogmask(int how, const sigset_t* set, sigset_t* oldset);
};

#endif //INCLUDE_PROCESS_H
