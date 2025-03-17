#ifndef INCLUDE_PROCESS_H
#define INCLUDE_PROCESS_H

#include "../core/memory.h"
#include "../core/interrupts.h"
#include "ELF.h"
#include "../utils/list.h"
#include <kstddef.h>

typedef uint pid_t;

// Process is ready to be executed
#define P_READY 0
// Process is terminated but not freed
#define P_TERMINATED 1
// Process has been interrupted during a syscall
#define P_SYSCALL_INTERRUPTED 2
// Process is waiting for a key press
#define P_WAITING_KEY 4

#define SEGFAULT_RET_VAL 255
#define GPF_RET_VAL 254
#define INIT_ERR_RET_VAL 253

class Scheduler;

class Process
{
	friend class Scheduler; // Scheduler managers processes, it needs complete access to do its stuff
	friend class ELFLoader; // ELFLoader creates processes, it acts like the constructor, it initializes most fields

private:
	// Process page tables. Process can use all virtual addresses below the kernel virtual location at pde 768
	Memory::page_table_t page_tables[768]{};
	Memory::pdt_t pdt{}; // process page directory table

	uint quantum, priority;

	uint num_pages; // Num pages over which the process code spans, including unmapped pages
	uint* pte; // Array of pte where the process code is loaded to

	pid_t pid; // PID
	pid_t ppid; // Parent PID

	uint k_stack_top; // Top of syscall handlers' stack
	uint flags; // Process state

	list<uint> allocs{}; // list of memory blocks allocated by the process

	int ret_val{};
public:
	struct elf_dependence_list* elf_dependence_list;
	cpu_state_t cpu_state{}; // Registers
	cpu_state_t k_cpu_state{}; // Syscall handler registers
	stack_state_t stack_state{}; // Execution context
	stack_state_t k_stack_state{}; // Syscall handler execution context

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
	void terminate(int ret_val);

	/**
	 * Contiguous heap memory allocator
	 * @param n required memory quantity
	 * @return pointer to the beginning of allocated heap memory, NULL if an error occurred
	 */
	[[nodiscard]] void* malloc(uint n);

	[[nodiscard]] void* calloc(size_t nmemb, size_t size);

	[[nodiscard]] void* realloc(void * ptr, size_t size);

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

	/**
	* Computes the runtime address of a symbol referenced by an ELF.
	* Works in conjunction with dynlk to resolve symbol addresses at runtime for lazy binding.
	* Search the symbol in the list containing the ELF and its dependencies.
	* @param dep ELF asking for the symbol address
	* @param symbol_name name of the symbol
	* @return runtime address of the symbol, 0x00 if not found
	*/
	static uint get_symbol_runtime_address(const struct elf_dependence_list* dep, const char* symbol_name);
};

#endif //INCLUDE_PROCESS_H
