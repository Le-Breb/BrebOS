#ifndef INCLUDE_PROCESS_H
#define INCLUDE_PROCESS_H

#include "../core/memory.h"
#include "../core/interrupts.h"
#include "ELF.h"
#include "../utils/list.h"
#include "../file_management/VFS.h"
#include <kstddef.h>

typedef int pid_t;

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

#define SEGFAULT_RET_VAL 255
#define GPF_RET_VAL 254
#define INIT_ERR_RET_VAL 253

class Scheduler;

class Process
{
	friend class Scheduler; // Scheduler managers processes, it needs complete access to do its stuff
	friend class ELFLoader; // ELFLoader creates processes, it acts like the constructor, it initializes most fields

	struct env_var
	{
		const char* name;
		char* value;
	};

	struct address_val_pair
	{
		void* address;
		int value;
	};

	uint quantum, priority;

	uint num_pages; // Num pages over which the process code spans, including unmapped pages

	pid_t pid; // PID
	pid_t ppid; // Parent PID

	uint k_stack_top; // Top of syscall handlers' stack
	uint flags; // Process state

	int ret_val{};

	list<void*> allocations{};

	Process* exec_replacement = nullptr; // Process which will replace this program after an call to execve

	VFS::file_descriptor* file_descriptors[MAX_FD_PER_PROCESS]{};
	// Lowest free file descriptor, 0, 1 and 2 are reserved for stdin, stdout and stderr. They are not implemented yet.
	// but its for POSIX compatibility
	int lowest_free_fd = 3;

	static list<env_var*> env_list; // Todo: make env var process specific

	void copy_page_to_other_process(const Process* other, uint page_id, uint mapping_page_id) const;

	void copy_page_to_other_process_shared(const Process* other, uint page_id) const;

public:
	uint lowest_free_pe;
	uint free_bytes = 0;

	list<elf_dependence>* elf_dep_list;
	cpu_state_t cpu_state{}; // Registers
	cpu_state_t k_cpu_state{}; // Syscall handler registers
	stack_state_t stack_state{}; // Execution context
	stack_state_t k_stack_state{}; // Syscall handler execution context

	bool is_waiting_for_any_child_to_terminate = false; // Tells whether the process is waiting for any child to terminate
	bool is_waited_by_parent = false; // Tells whether the parent specifically waited for this process to terminate
	list<pid_t> children{};
	list<address_val_pair> values_to_write{}; // list of values that need to be written in process address space

	Memory::memory_header mem_base{.s = {&mem_base, 0}};
	Memory::memory_header* freep = &mem_base; // list of memory blocks allocated by the process

	// Those fields have to be first for alignment constraints
	// Process page tables. Process can use all virtual addresses below the kernel virtual location at pde 768
	Memory::page_table_t* page_tables;
	Memory::pdt_t* pdt; // process page directory table

	Elf32_Addr program_break;

private:
	/** Frees a terminated process */
	~Process();

	Process(uint num_pages, list<elf_dependence>* elf_dep_list, Memory::page_table_t* page_tables,
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

public:
	/** Gets the process' PID */
	[[nodiscard]] pid_t get_pid() const;

	/**
	 * Terminates a process
	 */
	void terminate(int ret_val);

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

	/** Checks whether the program is terminated */
	[[nodiscard]] bool is_terminated() const;

	/** Checks whether the program is waiting for a key press */
	[[nodiscard]] bool is_waiting_key() const;

	/** Checks whether the program is waiting for another program to terminate */
	[[nodiscard]] bool is_waiting_program() const;

	[[nodiscard]] bool is_sleeping() const;

	[[nodiscard]] bool exec_running() const;

	/**
	* Computes the runtime address of a symbol referenced by an ELF.
	* Works in conjunction with dynlk to resolve symbol addresses at runtime for lazy binding.
	* Search the symbol in the list containing the ELF and its dependencies.
	* @param dep_id id of the elf in elf_dep_list
	* @param symbol_name name of the symbol
	* @return runtime address of the symbol, 0x00 if not found
	* @warning As its name suggests, this function should only be called during runtime of the ELF represented by this
	* process (because of the usage of the ELF runtime load address)
	*/
	uint get_symbol_runtime_address_at_runtime(uint dep_id, const char* symbol_name) const;

	static void init();

	static char* get_env(const char* name);

	static void set_env(const char* name, const char* value);

	pid_t fork();

	/**
	 * Updates a page table entry.
	 * @param pte page id
	 * @param val new page entry value
	 * @param update_cache whether TLB cache should be updated after applying the update
	 */
	void update_pte(uint pte, uint val, bool update_cache) const;

	void* sbrk(int increment);

	/**
	 * Transfer various data to proc, which is set as exec replacement
	 * @param proc process that whill replace this process
	 */
	void execve_transfer(Process* proc);

	/**
	 * Opens a file
	 * @param pathname path of the file to open
	 * @param flags opening parameters
	 * @return file identifier on success, -2 if system-wide fd limit is reached, -3 if bad fd, -4 if process fd limit reached
	 */
	int open(const char* pathname, int flags);

	/**
	 * Reads data from a file descriptor
	 * @param fd file descriptor to read from
	 * @param buf buffer to read data into
	 * @param count how many bytes to read
	 * @return number of bytes read on success, -2 if fd not open, -3 if not a regular file, -4 if IO error
	 */
	int read(int fd, void* buf, size_t count) const;

	/**
	 * Closes a file descriptor
	 * @param fd file descriptor to close
	 * @return 0 on success, -2 if fd not open
	 */
	int close(int fd);

	[[nodiscard]]
	int lseek(int fd, int offset, int whence) const;

	int proc_to_sys_fd(int fd) const;
};

#endif //INCLUDE_PROCESS_H
