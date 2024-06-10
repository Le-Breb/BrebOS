#include "process.h"
#include "clib/string.h"
#include "clib/stdio.h"
#include "gdt.h"
#include "memory.h"
#include "system.h"
#include "interrupts.h"

// Bitmap of available PID. Ith LSB indicates PID state ; 1 = used, 0 = free.
// Thus, 32 = sizeof(unsigned int) PIDs are available, allowing up to 32 processes to run concurrently
unsigned int pid_pool = 0;

pid running_process = 0x00;
ready_queue_t ready_queue;
process* processes[MAX_PROCESSES];

/** Change current pdt */
extern void change_pdt_asm(unsigned int pdt_phys_addr);

process* load_binary(unsigned int module, GRUB_module* grub_modules)
{
	unsigned int mod_size = grub_modules[module].size;
	unsigned int mod_code_pages = mod_size / PAGE_SIZE + ((mod_size % PAGE_SIZE) ? 1 : 0); // Num pages code spans over

	// Allocate process memory
	unsigned int proc_size = sizeof(process);

	process* proc = page_aligned_malloc(proc_size);
	proc->num_pages = mod_code_pages;
	proc->page_table_entries = malloc(mod_code_pages);

	// Allocate process code pages
	for (unsigned int k = 0; k < mod_code_pages; ++k)
	{
		unsigned int page_entry = get_free_page_entry();
		proc->page_table_entries[k] = page_entry;
		allocate_page_user(get_free_page(), page_entry);
	}


	unsigned int proc_page_entry_id = 0;
	unsigned int sys_page_entry_id = proc->page_table_entries[proc_page_entry_id];
	unsigned int code_page_start_addr = grub_modules[module].start_addr + proc_page_entry_id * PAGE_SIZE;
	unsigned int dest_page_start_addr =
			(sys_page_entry_id / PDT_ENTRIES) << 22 | (sys_page_entry_id % PDT_ENTRIES) << 12;

	// Copy whole pages
	for (; proc_page_entry_id < mod_size / PAGE_SIZE; ++proc_page_entry_id)
	{
		code_page_start_addr = grub_modules[module].start_addr + proc_page_entry_id * PAGE_SIZE;
		sys_page_entry_id = proc->page_table_entries[proc_page_entry_id];
		dest_page_start_addr = (sys_page_entry_id / PDT_ENTRIES) << 22 | (sys_page_entry_id % PDT_ENTRIES) << 12;
		memcpy((void*) dest_page_start_addr, (void*) (grub_modules[module].start_addr + code_page_start_addr),
			   PAGE_SIZE);
	}
	// Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
	memcpy((void*) dest_page_start_addr, (void*) code_page_start_addr, mod_size % PAGE_SIZE);
	memset((void*) (dest_page_start_addr + mod_size % PAGE_SIZE), 0, PAGE_SIZE - mod_size % PAGE_SIZE);

	return proc;
}

process* load_elf(unsigned int module, GRUB_module* grub_modules)
{
	Elf32_Ehdr* elf32Ehdr = (Elf32_Ehdr*) grub_modules[module].start_addr;

	// Ensure this is an ELF
	if (elf32Ehdr->e_ident[0] != 0x7F || elf32Ehdr->e_ident[1] != 'E' || elf32Ehdr->e_ident[2] != 'L' ||
		elf32Ehdr->e_ident[3] != 'F')
	{
		printf_error("Not an ELF\n");
		return 0x00;
	}

	// Ensure program is 32 bits
	if (elf32Ehdr->e_ident[4] != 1)
		printf_error("Not a 32 bit program");

	// Ensure program is an executable
	if (elf32Ehdr->e_type != ET_EXEC)
	{
		printf_error("Not an executable ELF");
		return 0x00;
	}
	//printf("Endianness: %s\n", elf32Ehdr->e_ident[5] == 1 ? "Little" : "Big");

	// Get section headers
	Elf32_Shdr* elf32Shdr = (Elf32_Shdr*) (grub_modules[module].start_addr + elf32Ehdr->e_shoff);

	// ELF imposes that first section header is null
	if (elf32Shdr->sh_type != SHT_NULL)
	{
		printf_error("Not a valid ELF file");
		return 0x00;
	}

	// Ensure program is supported (only simple statically linked programs supported yet)
	for (int k = 0; k < elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((unsigned int) elf32Shdr + elf32Ehdr->e_shentsize * k);
		switch (h->sh_type)
		{
			case SHT_NULL:
			case SHT_PROGBITS:
			case SHT_SYMTAB:
			case SHT_STRTAB:
				break;
			default:
				printf("Section type not supported: %i. Aborting\n", h->sh_type);
				return 0x00;
		}
	}

	if (elf32Ehdr->e_phnum == 0)
	{
		printf_error("No program header");
		return 0x00;
	}

	// Get program headers
	Elf32_Phdr* elf32Phdr = (Elf32_Phdr*) (grub_modules[module].start_addr + elf32Ehdr->e_phoff);

	unsigned proc_size = 0; // Code size in bytes
	for (int k = 0; k < elf32Ehdr->e_phnum; ++k)
	{
		if (elf32Phdr[k].p_type != PT_LOAD)
			continue;

		/*for (unsigned int l = 0; l < elf32Phdr[k].p_filesz; ++l)
			printf("%02x ", *(unsigned char*) (grub_modules[module].start_addr + elf32Phdr[k].p_offset + l));
		for (unsigned int l = 0; l < elf32Phdr[k].p_memsz - elf32Phdr[k].p_filesz; ++l)
			printf("%02x ", 0);*/

		proc_size += elf32Phdr[k].p_memsz;
	}

	// Allocate process struct
	process* proc = page_aligned_malloc(sizeof(process));
	unsigned int proc_code_pages =
			proc_size / PAGE_SIZE + ((proc_size % PAGE_SIZE) ? 1 : 0); // Num pages code spans over
	proc->num_pages = proc_code_pages;
	proc->page_table_entries = malloc(proc->num_pages * sizeof(unsigned int));

	// Allocate process code pages
	for (unsigned int k = 0; k < proc_code_pages; ++k)
	{
		unsigned int page_entry = get_free_page_entry(); // Get PTE id
		proc->page_table_entries[k] = page_entry;  // Reference PTE
		allocate_page_user(get_free_page(), page_entry); // Allocate page
	}

	// Copy process code in allocated space
	for (int k = 0; k < elf32Ehdr->e_phnum; ++k)
	{
		if (elf32Phdr[k].p_type != PT_LOAD)
			continue;

		unsigned int proc_page_entry_id = 0;
		unsigned int sys_page_entry_id = proc->page_table_entries[proc_page_entry_id];
		unsigned int code_page_start_addr =
				grub_modules[module].start_addr + elf32Phdr[k].p_offset + proc_page_entry_id * PAGE_SIZE;
		unsigned int dest_page_start_addr =
				(sys_page_entry_id / PDT_ENTRIES) << 22 | (sys_page_entry_id % PDT_ENTRIES) << 12;

		// Copy whole pages
		for (; proc_page_entry_id < elf32Phdr[k].p_filesz / PAGE_SIZE; ++proc_page_entry_id)
		{
			code_page_start_addr =
					grub_modules[module].start_addr + elf32Phdr[k].p_offset + proc_page_entry_id * PAGE_SIZE;
			sys_page_entry_id = proc->page_table_entries[proc_page_entry_id];
			dest_page_start_addr = (sys_page_entry_id / PDT_ENTRIES) << 22 | (sys_page_entry_id % PDT_ENTRIES) << 12;
			memcpy((void*) dest_page_start_addr, (void*) (grub_modules[module].start_addr + code_page_start_addr),
				   PAGE_SIZE);
		}
		// Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
		memcpy((void*) dest_page_start_addr, (void*) code_page_start_addr, elf32Phdr[k].p_filesz % PAGE_SIZE);
		memset((void*) (dest_page_start_addr + elf32Phdr[k].p_filesz % PAGE_SIZE), 0,
			   PAGE_SIZE - elf32Phdr[k].p_filesz % PAGE_SIZE);
	}

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
	unsigned int p_stack_page_entry_id = get_free_page();
	allocate_page_user(get_free_page(), p_stack_page_entry_id);

	// Allocate syscall handler stack page
	unsigned int k_stack_page_entry = get_free_page_entry();
	unsigned int k_stack_pde = k_stack_page_entry / PDT_ENTRIES;
	unsigned int k_stack_pte = k_stack_page_entry % PDT_ENTRIES;
	allocate_page(get_free_page(), k_stack_page_entry);

	// Set process pdt entries to target process page tables
	page_table_t* page_tables = get_page_tables();
	for (int i = 0; i < PDT_ENTRIES; ++i)
	{
		unsigned int pt_virt_addr = (unsigned int) &proc->page_tables[i];
		unsigned int pt_phys_addr = PHYS_ADDR(pt_virt_addr);
		proc->pdt.entries[i] = pt_phys_addr | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;
	}

	// Copy kernel page tables into process' ones
	memcpy(&proc->page_tables[0].entries[0], &page_tables[0].entries[0], PDT_ENTRIES * sizeof(page_table_t));

	// Map process code at 0x00
	for (unsigned int i = 0; i < proc->num_pages; ++i)
	{
		unsigned int page_pde = i / PDT_ENTRIES;
		unsigned int page_pte = i % PDT_ENTRIES;

		unsigned int process_page_id = proc->page_table_entries[i];
		unsigned int process_page_pde = process_page_id / PDT_ENTRIES;
		unsigned int process_page_pte = process_page_id % PDT_ENTRIES;

		// Swap process pages entries with first page entries to map process at 0x00
		if (proc->page_tables[page_pde].entries[page_pte] != 0)
			printf_error("Non empty pt entry");
		proc->page_tables[page_pde].entries[page_pte] = proc->page_tables[process_page_pde].entries[process_page_pte];
		proc->page_tables[process_page_pde].entries[process_page_pte] = 0;
	}

	// Map process stack at 0xBFFFFFFC = 0xCFFFFFFF - 4 at pde 767 and pte 1023, just below the kernel (move current entry)
	if (proc->page_tables[767].entries[1023] != 0)
		printf_error("Process kernel page is not empty");
	proc->page_tables[767].entries[1023] = PTE(p_stack_page_entry_id); // Copy entry - set correct stack virtual address
	PTE(p_stack_page_entry_id) = 0; // Remove previous entry

	// Setup PCB
	processes[pid] = proc;
	proc->pid = pid;
	proc->ppid = ppid;
	proc->k_stack_top = VIRT_ADDR(k_stack_pde, k_stack_pte, (PAGE_SIZE - 4));
	proc->stack_state.eip = 0;
	proc->stack_state.ss = 0x20 | 0x03;
	proc->stack_state.cs = 0x18 | 0x03;
	proc->stack_state.esp = 0xBFFFFFFC;
	proc->stack_state.eflags = 0x200;
	proc->stack_state.error_code = 0;
	memset(&proc->cpu_state, 0, sizeof(proc->cpu_state));

	// Set process ready
	add_process_to_ready_queue(pid);
}

void process_exit(pid pid, const unsigned int* stack_top_ptr)
{
	process* process = processes[pid];

	if (process == 0x00)
	{
		printf_error("No process with PID %u", pid);
		return;
	}

	// Switch back to kernel stack (as we are currently on process' interrupt stack)
	asm volatile("mov %0, %%esp" : : "r" (stack_top_ptr));
	// Switch back to kernel pdt
	page_table_t* page_tables = get_page_tables();
	change_pdt_asm(PHYS_ADDR((unsigned int) get_pdt()));

	// Free process code
	for (unsigned int i = 0; i < process->num_pages; ++i)
		free_page(process->page_table_entries[i] / PDT_ENTRIES,
				  process->page_table_entries[i] % PDT_ENTRIES);

	// Free process struct
	free(process->page_table_entries);
	page_aligned_free(process);

	release_pid(pid);
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
	return processes[running_process];
}

void schedule()
{
	// Nothing to run
	if (ready_queue.count == 0)
		shutdown();

	// Get process
	pid pid = running_process = ready_queue.arr[ready_queue.start];
	process* p = processes[pid];

	// Update queue
	ready_queue.count--;
	ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;

	// Use process' pdt
	page_table_t* page_tables = get_page_tables();
	unsigned int pdt_phys_addr = PHYS_ADDR((unsigned int) &p->pdt);
	change_pdt_asm(pdt_phys_addr);

	// Set TSS esp0 to point to the trap handler stack (i.e. tell the CPU where is trap handler stack)
	set_tss_kernel_stack(p->k_stack_top);

	// Jump
	user_process_jump_asm(p->cpu_state, p->stack_state);
}

void add_process_to_ready_queue(pid pid)
{
	ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = pid;
}

void release_pid(pid pid)
{
	pid_pool |= 1 << pid;
}
