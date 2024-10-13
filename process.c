#include "process.h"
#include "clib/string.h"
#include "clib/stdio.h"
#include "gdt.h"
#include "memory.h"
#include "system.h"
#include "interrupts.h"
#include "elf_tools.h"

// Bitmap of available PID. Ith LSB indicates PID state ; 1 = used, 0 = free.
// Thus, 32 = sizeof(unsigned int) PIDs are available, allowing up to 32 processes to run concurrently
unsigned int pid_pool = 0;

pid running_process = MAX_PROCESSES;
ready_queue_t ready_queue;
ready_queue_t waiting_queue;
process* processes[MAX_PROCESSES];

/** Change current pdt */
extern void change_pdt_asm(unsigned int pdt_phys_addr);

/**
 * Round-robin scheduler
 * @return next process to run, NULL if there is no process to run
 */
process* get_next_process();

process* load_binary(unsigned int module, GRUB_module* grub_modules)
{
	unsigned int mod_size = grub_modules[module].size;
	unsigned int mod_code_pages = mod_size / PAGE_SIZE + ((mod_size % PAGE_SIZE) ? 1 : 0); // Num pages code spans over

	// Allocate process memory
	unsigned int proc_size = sizeof(process);

	process* proc = page_aligned_malloc(proc_size);
	proc->stack_state.eip = 0;
	proc->num_pages = mod_code_pages;
	proc->pte = malloc(mod_code_pages * sizeof(unsigned int));

	// Allocate process code pages
	for (unsigned int k = 0; k < mod_code_pages; ++k)
	{
		unsigned int pe = get_free_pe();
		proc->pte[k] = pe;
		allocate_page_user(get_free_page(), pe);
	}


	unsigned int proc_pe_id = 0;
	unsigned int sys_pe_id = proc->pte[proc_pe_id];
	unsigned int code_page_start_addr = grub_modules[module].start_addr + proc_pe_id * PAGE_SIZE;
	unsigned int dest_page_start_addr =
			(sys_pe_id / PDT_ENTRIES) << 22 | (sys_pe_id % PDT_ENTRIES) << 12;

	// Copy whole pages
	for (; proc_pe_id < mod_size / PAGE_SIZE; ++proc_pe_id)
	{
		code_page_start_addr = grub_modules[module].start_addr + proc_pe_id * PAGE_SIZE;
		sys_pe_id = proc->pte[proc_pe_id];
		dest_page_start_addr = (sys_pe_id / PDT_ENTRIES) << 22 | (sys_pe_id % PDT_ENTRIES) << 12;
		memcpy((void*) dest_page_start_addr, (void*) (grub_modules[module].start_addr + code_page_start_addr),
			   PAGE_SIZE);
	}
	// Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
	memcpy((void*) dest_page_start_addr, (void*) code_page_start_addr, mod_size % PAGE_SIZE);
	memset((void*) (dest_page_start_addr + mod_size % PAGE_SIZE), 0, PAGE_SIZE - mod_size % PAGE_SIZE);

	return proc;
}

void start_module(unsigned int module, pid ppid)
{
	if (ready_queue.count == MAX_PROCESSES)
	{
		printf_error("Max process are already running");
		return;
	}

	pid pid = get_free_pid();
	if (pid == MAX_PROCESSES)
	{
		printf_error("No more PID available");
		return;
	}

	GRUB_module* grub_modules = get_grub_modules();

	// Check whether file is an elf
	Elf32_Ehdr* elf32Ehdr = (Elf32_Ehdr*) grub_modules[module].start_addr;
	unsigned int elf = elf32Ehdr->e_ident[0] == 0x7F && elf32Ehdr->e_ident[1] == 'E' && elf32Ehdr->e_ident[2] == 'L' &&
					   elf32Ehdr->e_ident[3] == 'F';

	// Load process into memory
	process* proc = elf ? load_elf(module, grub_modules) : load_binary(module, grub_modules);

	// Ensure process was loaded successfully
	if (proc == 0x00)
	{
		printf_error("Error while loading program. Aborting");
		return;
	}

	// Allocate process stack page
	unsigned int p_stack_pe_id = get_free_pe();
	allocate_page_user(get_free_page(), p_stack_pe_id);

	// Allocate syscall handler stack page
	unsigned int k_stack_pe = get_free_pe();
	unsigned int k_stack_pde = k_stack_pe / PDT_ENTRIES;
	unsigned int k_stack_pte = k_stack_pe % PDT_ENTRIES;
	allocate_page(get_free_page(), k_stack_pe);

	// Set process PDT entries: entries 0 to 767 map the process address space using its own page tables
	// Entries 768 to 1024 point to kernel page tables, so that kernel is mapped. Moreover, syscall handlers
	// will not need to switch to kernel pdt to make changes in kernel memory as it is mapped the same way
	// in every process' PDT
	// Set process pdt entries to target process page tables
	page_table_t* page_tables = get_page_tables();
	for (int i = 0; i < 768; ++i)
		proc->pdt.entries[i] = PHYS_ADDR((unsigned int) &proc->page_tables[i]) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;
	// Use kernel page tables for the rest
	for (int i = 768; i < PDT_ENTRIES; ++i)
		proc->pdt.entries[i] = PHYS_ADDR((unsigned int) &page_tables[i]) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;

	// Map process code at 0x00
	for (unsigned int i = 0; i < proc->num_pages; ++i)
	{
		unsigned int page_pde = i / PDT_ENTRIES;
		unsigned int page_pte = i % PDT_ENTRIES;

		unsigned int process_page_id = proc->pte[i];
		unsigned int process_page_pde = process_page_id / PDT_ENTRIES;
		unsigned int process_page_pte = process_page_id % PDT_ENTRIES;

		if (proc->page_tables[page_pde].entries[page_pte] != 0)
			printf_error("Non empty pt entry");
		proc->page_tables[page_pde].entries[page_pte] = page_tables[process_page_pde].entries[process_page_pte];
	}

	// Map process stack at 0xBFFFFFFC = 0xCFFFFFFF - 4 at pde 767 and pte 1023, just below the kernel
	if (proc->page_tables[767].entries[1023] != 0)
		printf_error("Process kernel page entry is not empty");
	proc->page_tables[767].entries[1023] = PTE(p_stack_pe_id);

	// Setup PCB
	processes[pid] = proc;
	proc->pid = pid;
	proc->ppid = ppid;
	proc->priority = 1;
	proc->k_stack_top = VIRT_ADDR(k_stack_pde, k_stack_pte, (PAGE_SIZE - sizeof(int)));
	//proc->stack_state.eip = 0;
	proc->stack_state.ss = 0x20 | 0x03;
	proc->stack_state.cs = 0x18 | 0x03;
	proc->stack_state.esp = 0xBFFFFFFC;
	proc->stack_state.eflags = 0x200;
	proc->stack_state.error_code = 0;
	proc->allocs = list_init();
	memset(&proc->cpu_state, 0, sizeof(proc->cpu_state));

	// Set process ready
	set_process_ready(pid);
}

void free_process(pid pid)
{
	process* p = processes[pid];

	if (p == 0x00)
	{
		printf_error("No process with PID %u", pid);
		return;
	}

	if (!(p->flags & P_TERMINATED))
	{
		printf_error("Process %u is not terminated", pid);
		return;
	}

	// Free process code
	for (unsigned int i = 0; i < p->num_pages; ++i)
		free_page(p->pte[i] / PDT_ENTRIES,
				  p->pte[i] % PDT_ENTRIES);

	// Free process struct
	free(p->pte);
	page_aligned_free(p);
	free(p->elfi);

	release_pid(pid);
	p->flags |= P_TERMINATED;

	printf_info("Process %u exited", pid);
}

pid get_free_pid()
{
	for (unsigned int i = 0; i < MAX_PROCESSES; ++i)
	{
		if (!(pid_pool & (1 << i)))
		{
			pid_pool |= (1 << i); // Mark PID as used
			return i;
		}
	}

	return MAX_PROCESSES; // No PID available
}

void processes_init()
{
	for (unsigned int i = 0; i < MAX_PROCESSES; ++i)
	{
		ready_queue.arr[i] = -1;
		processes[i] = 0x00;
	}
	ready_queue.start = ready_queue.count = 0;
}

pid get_running_process_pid()
{
	return running_process;
}

process* get_running_process()
{
	return running_process == MAX_PROCESSES ? 0x00 : processes[running_process];
}

void wake_up_key_waiting_processes(char key)
{
	for (unsigned int i = 0; i < waiting_queue.count; ++i)
	{
		// Add process to ready queue
		pid pid = waiting_queue.arr[(waiting_queue.start + waiting_queue.count) % MAX_PROCESSES];
		ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = pid;

		processes[pid]->cpu_state.eax = (unsigned int) key; // Return key
		processes[pid]->flags &= ~P_WAITING_KEY; // Clear flag
	}

	// Clear waiting queue
	waiting_queue.count = 0;
}

process* get_next_process()
{
	process* p = 0x00;
	running_process = MAX_PROCESSES;

	// Find next process to execute
	while (p == 0x00)
	{
		// Nothing to run
		if (ready_queue.count == 0)
			return 0x00;

		// Get process
		pid pid = running_process = ready_queue.arr[ready_queue.start];
		process* proc = processes[pid];

		if (proc->flags & P_TERMINATED)
		{
			free_process(proc->pid);
			ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
			ready_queue.count--;
		}
		else if (proc->flags & P_WAITING_KEY)
		{
			ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
			ready_queue.count--;
			waiting_queue.arr[(waiting_queue.start + waiting_queue.count) % MAX_PROCESSES] = proc->pid;
			waiting_queue.count++;
		}
		else
		{
			p = proc;

			// Update queue
			if (p->quantum)
				p->quantum -= CLOCK_TICK_MS;
			else
			{
				ready_queue.arr[(ready_queue.start + ready_queue.count) % MAX_PROCESSES] = p->pid;
				RESET_QUANTUM(p);
				ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
			}
		}
	}

	return p;
}

_Noreturn void schedule()
{
	process* p = get_next_process();

	if (p == 0x00)
	{
		if (waiting_queue.count == 0)
			shutdown();
		else
		{
			// All processes are waiting for a key press. Thus, we can halt the CPU
			running_process = MAX_PROCESSES; // Indicate that no process is running

			__asm__ volatile("mov %0, %%esp" : : "r"(get_stack_top_ptr())); // Use global kernel stack
			__asm__ volatile("sti"); // Make sure interrupts are enabled
			__asm__ volatile("hlt"); // Halt
		}
	}

	page_table_t* page_tables = get_page_tables();

	// Set TSS esp0 to point to the syscall handler stack (i.e. tell the CPU where is syscall handler stack)
	set_tss_kernel_stack(p->k_stack_top);

	// Use process' address space
	change_pdt_asm(PHYS_ADDR((unsigned int) &p->pdt));

	// Jump
	if (p->flags & P_SYSCALL_INTERRUPTED)
	{
		p->flags &= ~P_SYSCALL_INTERRUPTED;
		resume_syscall_handler_asm(p->k_cpu_state, p->k_stack_state.esp - 12);
	}
	else
		resume_user_process_asm(p->cpu_state, p->stack_state);
}

void set_process_ready(pid pid)
{
	ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = pid;
	RESET_QUANTUM(processes[pid]);
}

void release_pid(pid pid)
{
	pid_pool &= ~(1 << pid);
}

void terminate_process(process* p)
{
	p->flags |= P_TERMINATED;
}

void* process_allocate_dyn_memory(process* p, uint n)
{
	void* mem = malloc(n);

	if (mem)
	{
		if (!list_push_front(p->allocs, mem))
		{
			free(mem);
			return NULL;
		}
	}

	return mem;
}

void process_free_dyn_memory(process* p, void* ptr)
{
	int pos = list_find(p->allocs, ptr);
	if (pos == -1)
		printf_error("Process dyn memory ptr not found in process allocs");
	list_remove_at(p->allocs, pos);
	free(ptr);
}
