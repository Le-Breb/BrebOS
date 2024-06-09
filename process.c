#include "process.h"
#include "lib/string.h"
#include "lib/stdio.h"
#include "gdt.h"
#include "memory.h"
#include "system.h"
#include "interrupts.h"

/** Change current pdt */
extern void change_pdt_asm(unsigned int pdt_phys_addr);

process* running_process = 0x00;

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

void run_module(unsigned int module, GRUB_module* grub_modules, page_table_t* page_tables)
{
	// Check whether file is an elf
	Elf32_Ehdr* elf32Ehdr = (Elf32_Ehdr*) grub_modules[module].start_addr;
	unsigned int elf = elf32Ehdr->e_ident[0] == 0x7F && elf32Ehdr->e_ident[1] == 'E' && elf32Ehdr->e_ident[2] == 'L' &&
					   elf32Ehdr->e_ident[3] == 'F';

	process* proc = elf ? load_elf(module, grub_modules) : load_binary(module,
																	   grub_modules); // Load process into memory

	// Ensure process was loaded successfully
	if (proc == 0x00)
	{
		printf_error("Error while loading program. Aborting");
		return;
	}

	// Allocate process stack page
	unsigned int p_stack_page_entry_id = get_free_page();
	unsigned int p_stack_pde = p_stack_page_entry_id / PDT_ENTRIES;
	unsigned int p_stack_pte = p_stack_page_entry_id % PDT_ENTRIES;
	allocate_page_user(get_free_page(), p_stack_page_entry_id);

	// Allocate syscall handler stack page
	unsigned int k_stack_page_entry = get_free_page_entry();
	unsigned int k_stack_pde = k_stack_page_entry / PDT_ENTRIES;
	unsigned int k_stack_pte = k_stack_page_entry % PDT_ENTRIES;
	allocate_page(get_free_page(), k_stack_page_entry);

	// Set process pdt entries to target process page tables
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

	unsigned int p_stack_top = VIRT_ADDR(p_stack_pde, p_stack_pte, (PAGE_SIZE - 4)); // process stack top
	unsigned int k_stack_top = VIRT_ADDR(k_stack_pde, k_stack_pte, (PAGE_SIZE - 4)); // kernel stack top

	// Set TSS esp0 to point to the trap handler stack (i.e. tell the CPU where is trap handler stack)
	set_tss_kernel_stack(k_stack_top);

	// Jump
	running_process = proc;
	unsigned int proc_pdt_phys_addr = PHYS_ADDR(((unsigned int) &proc->pdt)); // Process pdt physical address
	unsigned int eip = 0; // 0 because process is mapped at 0x00
	unsigned int user_code_seg_sel = 0x18 | 0x03; // 3rd entry, pl3
	unsigned int user_data_seg_sel = 0x20 | 0x03; // 4th entry, pl3
	unsigned int eflags = 0x200; // IF = 1
	user_mode_jump_asm(proc_pdt_phys_addr, eip, user_code_seg_sel, eflags, p_stack_top, user_data_seg_sel);
}

void process_exit(pdt_t* pdt, page_table_t* page_tables, const unsigned int* stack_top_ptr)
{
	// Free process code
	for (unsigned int i = 0; i < running_process->num_pages; ++i)
		free_page(running_process->page_table_entries[i] / PDT_ENTRIES,
				  running_process->page_table_entries[i] % PDT_ENTRIES);

	// Free process struct
	free(running_process->page_table_entries);
	page_aligned_free(running_process);

	running_process = 0x00;

	// Switch back to kernel stack (as we are currently on process' interrupt stack)
	asm volatile("mov %0, %%esp" : : "r" (stack_top_ptr));

	// Switch back to kernel pdt
	change_pdt_asm(PHYS_ADDR((unsigned int) pdt));
	printf_info("Process exited");

	shutdown();
}

process* get_running_process()
{
	return running_process;
}
