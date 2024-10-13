#ifndef INCLUDE_PROCESS_H
#define INCLUDE_PROCESS_H

#include "memory.h"
#include "interrupts.h"
#include "elf.h"
#include "list.h"
#include "clib/stddef.h"

typedef unsigned int pid;

// Maximum concurrent processes. Limit defined by size of pid_pool
#define MAX_PROCESSES (sizeof(unsigned int) * 8)

typedef struct
{
	pid arr[MAX_PROCESSES];
	unsigned int start, count;
} ready_queue_t;

#define P_TERMINATED 1
#define P_SYSCALL_INTERRUPTED 2
#define P_WAITING_KEY 4

#define RESET_QUANTUM(p) (p->quantum = p->priority * CLOCK_TICK_MS)

#define OS_INTERPR ("/dynlk") // OS default interpreter, used to run dynamically linked programs
#define OS_LIB ("libsyscalls.so") // OS lib, allowing programs to use syscalls


typedef struct
{
	Elf32_Ehdr* elf32Ehdr;
	Elf32_Phdr* elf32Phdr;
	Elf32_Shdr* elf32Shdr;
	GRUB_module* mod;
} elf_info;

typedef struct
{
	// Process page tables. Process can use all virtual addresses below the kernel virtual location at pde 768
	page_table_t page_tables[768];
	pdt_t pdt;

	unsigned int quantum, priority;

	unsigned int num_pages; // Num pages over which the process code spans
	unsigned int* pte; // Array of pte where the process code is loaded to

	pid pid; // PID
	pid ppid; // Parent PID

	unsigned int k_stack_top; // Top of syscall handlers' stack
	unsigned int flags;

	cpu_state_t cpu_state; // Registers
	cpu_state_t k_cpu_state; // Syscall handler registers
	stack_state_t stack_state; // Execution context
	stack_state_t k_stack_state; // Syscall handler execution context

	list* allocs; // list of memory blocks allocated by the process

	elf_info* elfi;
} process;


/**
 * Load a flat binary in memory
 *
 * @param module Grub module id
 * @param grub_modules grub modules
 * @return program's process
 */
process* load_binary(unsigned int module, GRUB_module* grub_modules);

/**
 * Loads a GRUB module and set it ready
 *
 * @param module Grub module id
 */
void start_module(unsigned int module, pid ppid);

/**
 * Terminates a process
 */
void terminate_process(process* p);

/** Frees a terminated process */
void free_process(pid pid);

/**
 * Tries to get a free PID from pid_pool
 * @return 0 if all PIDs are used, a valid PID otherwise
 */
pid get_free_pid();

/**
 * Indicates that a used PID is now free
 * @param pid PID to free
 */
void release_pid(pid pid);

/** Initialize processes handling data structures */
void processes_init();

/**
 * Gets the PID of current running process
 * @return PID of current running process
 */
pid get_running_process_pid();

/**
 * Gets running process
 * @return running process
 */
process* get_running_process();

/**
 * Executes next process in the ready queue
 */
_Noreturn void schedule();

/**
 * Adds a process to the ready queue
 * @param pid Process' PID
 */
void set_process_ready(pid pid);

/**
 * Remove processes waiting for a key press from waiting list and add them to ready queue
 * @param key Key pressed
 */
void wake_up_key_waiting_processes(char key);

void* process_allocate_dyn_memory(process* p, uint n);

void process_free_dyn_memory(process* p, void* ptr);

#endif //INCLUDE_PROCESS_H
