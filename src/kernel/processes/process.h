#ifndef INCLUDE_PROCESS_H
#define INCLUDE_PROCESS_H
#include "../core/memory.h"
#include "../core/interrupts.h"
#include "ELF.h"
#include "../utils/list.h"
#include <kstddef.h>

#include "scheduler.h"
#include "thread.h"


#define MAX_THREADS (sizeof(uint) * 8)

typedef uint pid_t;

// Process is ready to be executed
#define P_READY 0
// Process is terminated but not freed
#define P_TERMINATED 1
// Process has been interrupted during a syscall
#define P_SYSCALL_INTERRUPTED 2
// Process is waiting for a key press
#define P_WAITING_KEY 4


class Scheduler;

class Process
{
	friend class Scheduler; // Scheduler managers processes, it needs complete access to do its stuff
	friend class ELFLoader; // ELFLoader creates processes, it acts like the constructor, it initializes most fields

private:
	// Process page tables. Process can use all virtual addresses below the kernel virtual location at pde 768
	page_table_t page_tables[768]{};
	pdt_t pdt{}; // process page directory table

	uint quantum, priority;

	uint num_pages; // Num pages over which the process code spans, including unmapped pages
	uint* pte; // Array of pte where the process code is loaded to

	pid_t pid; // PID
	pid_t ppid; // Parent PID
	tid_t current_thread = 0;

	uint flags; // Process state

	list<uint> allocs{}; // list of memory blocks allocated by the process

	Thread* threads[MAX_THREADS];
public:

	struct elf_dependence_list* elf_dependence_list;

private:
	/** Page aligned allocator **/
	void* operator new(size_t size);

	/** Page aligned allocator **/
	void* operator new[](size_t size);

	/** Page aligned free **/
	void operator delete(void* p);

	/** Page aligned free **/
	void operator delete[](void* p);

	/** Frees a terminated process */
	~Process();

	explicit Process(uint num_pages, ELF* elf, Elf32_Addr runtime_load_address, const char* path);

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
	void terminate();

	/**
	 * Contiguous heap memory allocator
	 * @param n required memory quantity
	 * @return pointer to the beginning of allocated heap memory, NULL if an error occurred
	 */
	[[nodiscard]] void* allocate_dyn_memory(uint n);

	/**
	 * Contiguous heap memory free
	 * @param ptr pointer to the beginning of allocated heap memory
	 */
	void free_dyn_memory(void* ptr);

	/**
	 * Sets a flag
	 * @param flag flag to set
	 */
	void set_flag(uint flag);



	/** Checks whether the program is waiting for a key press */

	/**
	* Computes the runtime address of a symbol referenced by an ELF.
	* Works in conjunction with dynlk to resolve symbol addresses at runtime for lazy binding.
	* Search the symbol in the list containing the ELF and its dependencies.
	* @param dep ELF asking for the symbol address
	* @param symbol_name name of the symbol
	* @return runtime address of the symbol, 0x00 if not found
	*/
	static uint get_symbol_runtime_address(const struct elf_dependence_list* dep, const char* symbol_name);

	[[nodiscard]] Thread* get_current_thread() const;

	[[nodiscard]] tid_t get_current_thread_tid() const;

	[[nodiscard]] Thread* next_thread() const;

	void set_thread_pid(pid_t pid, Thread* thread);
};

#endif //INCLUDE_PROCESS_H
