#include "process.h"
#include "clib/string.h"
#include "clib/stdio.h"
#include "memory.h"
#include "PIT.h"

/** Change current pdt */
extern "C" void change_pdt_asm_(unsigned int pdt_phys_addr);

Process* Process::from_binary(GRUB_module* module)
{
	unsigned int mod_size = module->size;
	unsigned int mod_code_pages = mod_size / PAGE_SIZE + ((mod_size % PAGE_SIZE) ? 1 : 0); // Num pages code spans over

	// Allocate process memory
	Process* proc = new Process();
	proc->stack_state.eip = 0;
	proc->num_pages = mod_code_pages;
	proc->pte = (uint*) malloc(mod_code_pages * sizeof(unsigned int));

	// Allocate process code pages
	for (unsigned int k = 0; k < mod_code_pages; ++k)
	{
		unsigned int pe = get_free_pe();
		proc->pte[k] = pe;
		allocate_page_user(get_free_page(), pe);
	}


	unsigned int proc_pe_id = 0;
	unsigned int sys_pe_id = proc->pte[proc_pe_id];
	unsigned int code_page_start_addr = module->start_addr + proc_pe_id * PAGE_SIZE;
	unsigned int dest_page_start_addr =
			(sys_pe_id / PDT_ENTRIES) << 22 | (sys_pe_id % PDT_ENTRIES) << 12;

	// Copy whole pages
	for (; proc_pe_id < mod_size / PAGE_SIZE; ++proc_pe_id)
	{
		code_page_start_addr = module->start_addr + proc_pe_id * PAGE_SIZE;
		sys_pe_id = proc->pte[proc_pe_id];
		dest_page_start_addr = (sys_pe_id / PDT_ENTRIES) << 22 | (sys_pe_id % PDT_ENTRIES) << 12;
		memcpy((void*) dest_page_start_addr, (void*) (module->start_addr + code_page_start_addr),
			   PAGE_SIZE);
	}
	// Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
	memcpy((void*) dest_page_start_addr, (void*) code_page_start_addr, mod_size % PAGE_SIZE);
	memset((void*) (dest_page_start_addr + mod_size % PAGE_SIZE), 0, PAGE_SIZE - mod_size % PAGE_SIZE);

	return proc;
}

Process::~Process()
{
	if (!(flags & P_TERMINATED))
	{
		printf_error("Process %u is not terminated", pid);
		return;
	}

	// Free process code
	for (unsigned int i = 1; i < num_pages; ++i) // Skip page 0 as it is unmapped, use for segfaults
		free_page(pte[i] / PDT_ENTRIES,
				  pte[i] % PDT_ENTRIES);

	// Free process fields
	delete pte;
	delete elf;
	delete allocs;

	flags |= P_TERMINATED;

	printf_info("Process %u exited", pid);
}


void Process::terminate()
{
	flags |= P_TERMINATED;
}

void* Process::allocate_dyn_memory(uint n) const
{
	void* mem = malloc(n);

	if (mem)
	{
		if (!allocs->push_front(mem))
		{
			free(mem);
			return NULL;
		}
	}

	return mem;
}

void Process::free_dyn_memory(void* ptr) const
{
	int pos = allocs->find(ptr);
	if (pos == -1)
		printf_error("Process dyn memory ptr not found in process allocs");
	allocs->remove_at(pos);
	free(ptr);
}

void* Process::operator new(size_t size)
{
	return page_aligned_malloc(size);
}

void* Process::operator new[]([[maybe_unused]]size_t size)
{
	printf_error("Please do not use this operator");
	return (void*) 1;
}

void Process::operator delete(void* p)
{
	page_aligned_free(p);
}

void Process::operator delete[]([[maybe_unused]] void* p)
{
	printf_error("do not call this");
}

bool Process::dynamic_loading()
{
	GRUB_module* grub_modules = get_grub_modules();
	char* interpereter_name = elf->get_interpreter_name();
	if (strcmp(interpereter_name, OS_INTERPR) != 0)
	{
		printf_error("Unsupported interpreter: %s", interpereter_name);
		return false;
	}

	Elf32_Dyn* dyn_table = elf->get_dyn_table();
	if (dyn_table == nullptr)
		return false;
	size_t base_addr = elf->base_address();
	if (base_addr < PAGE_SIZE)
	{
		printf_error("Process base address (%zu) < PAGE_SIZE (%u). Aborting", base_addr, PAGE_SIZE);
		return false;
	}
	if (base_addr == (size_t) -1)
	{
		printf_error("Cannot compute base address. Aborting");
		return false;
	}

	// Get dynamic linking info
	unsigned int lib_name_idx = 0; // lib name string table index
	char* dyn_str_table = nullptr; // string table
	void* dynsymtab = nullptr; // Dynamic symbols table
	unsigned int dynsyntab_num_entries; // Dynamic symbols table number of entries
	unsigned int dynsymtab_ent_size; // Dynamic symbols table entry size
	unsigned int* file_got_addr; // Address of GOT within the ELF file (not where it is loaded nor its runtime address)
	// In order to know whether base address should be retrieved from values, refer to Figure 2-10: Dynamic Array Tags
	// in http://www.skyfree.org/linux/references/ELF_Format.pdf
	for (Elf32_Dyn* d = dyn_table; d->d_tag != DT_NULL; d++)
	{
		switch (d->d_tag)
		{
			case DT_STRTAB:
				dyn_str_table = (char*) (elf->mod->start_addr + d->d_un.d_ptr - base_addr);
				break;
			case DT_SYMTAB:
				dynsymtab = (void*) (elf->mod->start_addr + d->d_un.d_ptr - base_addr);
				break;
			case DT_NEEDED:
				lib_name_idx = d->d_un.d_val;
				break;
			case DT_PLTGOT:
				file_got_addr = (unsigned int*) (elf->mod->start_addr + d->d_un.d_val - base_addr);
				break;
			case DT_HASH:
				dynsyntab_num_entries = *(((unsigned int*) (elf->mod->start_addr + d->d_un.d_val - base_addr)) +
										  1); // nchain
				break;
			case DT_SYMENT:
				dynsymtab_ent_size = d->d_un.d_val;
				break;
			case DT_GNU_HASH:
			case DT_STRSZ:
			case DT_DEBUG:
			case DT_PLTRELSZ:
			case DT_PLTREL:
			case DT_JMPREL:
				break;
			default:
				printf_error("Unknown dynamic table entry: %u|%x", d->d_tag, d->d_tag);
				break;
		}
	}
	if (dyn_str_table == nullptr)
	{
		printf_error("No dynamic string table");
		return false;
	}
	if (dynsymtab == nullptr)
	{
		printf_error("No dynamic symbol table");
		return false;
	}
	if (file_got_addr == nullptr)
	{
		printf_error("No GOT");
		return false;
	}

	// Get needed lib name and check it is not missing
	char* lib_name = &dyn_str_table[lib_name_idx];
	if (strcmp(lib_name, OS_LIB) != 0)
		printf_error("Missing library: %s\n", lib_name);

	// Ensure dynamic symbols are supported
	for (uint i = 1; i < dynsyntab_num_entries; ++i) // First entry has to be null, we skip it
	{
		Elf32_Sym* dd = (Elf32_Sym*) ((uint) dynsymtab + i * dynsymtab_ent_size);
		//printf("%s\n", &dyn_str_table[dd->st_name]);
		unsigned int type = ELF32_ST_TYPE(dd->st_info);
		if (type != STT_FUNC)
		{
			printf_error("Unsupported symbol type: %u", type);
			return false;
		}
	}

	Elf32_Phdr* got_segment_hdr = elf->get_GOT_segment(file_got_addr);
	if (got_segment_hdr == nullptr)
		return false;

	// Compute GOT runtime address and loaded GOT address
	unsigned int got_runtime_addr =
			(unsigned int) file_got_addr - (elf->mod->start_addr + got_segment_hdr->p_offset) +
			got_segment_hdr->p_vaddr;
	unsigned int got_sys_pe_id = pte[got_runtime_addr / PAGE_SIZE];
	unsigned int* got_addr = (unsigned int*) VIRT_ADDR(got_sys_pe_id / PDT_ENTRIES, got_sys_pe_id % PT_ENTRIES,
													   got_runtime_addr % PAGE_SIZE);

	// Load klib
	if (!load_lib(&grub_modules[3]))
		return false;
	// Load libdynlk
	unsigned int proc_num_pages = num_pages;
	if (!load_lib(&grub_modules[2]))
		return false;

	// Get libdynlk entry point
	ELF libdynlk_elf(&grub_modules[2]);
	void* libdynlk_entry_point = libdynlk_elf.get_libdynlk_main_runtime_addr(proc_num_pages);
	if (libdynlk_entry_point == nullptr)
	{
		printf_error("Error while loading libdynlk");
		return false;
	}

	// GOT setup: GOT[0] unused, GOT[1] = addr of GRUB module (to identify the program), GOT[2] = dynamic linker address
	*(void**) (got_addr + 1) = (void*) this;
	*(void**) (got_addr + 2) = libdynlk_entry_point;

	// Indicate entry point
	stack_state.eip = elf->elf32Ehdr->e_entry;

	return true;
}

bool Process::load_lib(GRUB_module* lib_mod)
{
	if (!ELF::is_valid(lib_mod, SharedObject))
		return false;

	unsigned int proc_num_pages = num_pages;
	ELF lib_elf(lib_mod);

	// Allocate space for lib int process' address space
	if (!alloc_and_add_lib_pages_to_process(lib_elf))
		return false;
	// Load lib into newly allocated space
	lib_elf.load_into_process_address_space(pte + proc_num_pages);

	return true;
}

bool Process::alloc_and_add_lib_pages_to_process(ELF& lib_elf)
{
	unsigned int highest_addr = lib_elf.get_highest_runtime_addr();
	// Num pages lib  code spans over
	unsigned int lib_num_code_pages = highest_addr / PAGE_SIZE + ((highest_addr % PAGE_SIZE) ? 1 : 0);

	// Adjust proc pte array - add space for lib code
	unsigned int* new_pte = (uint*) malloc((num_pages + lib_num_code_pages) * sizeof(unsigned int));
	if (new_pte == nullptr)
	{
		printf_error("Unable to allocate memory for process");
		return false;
	}
	memcpy(new_pte, pte, num_pages * sizeof(unsigned int));
	delete pte;
	pte = new_pte;

	// Allocate lib code pages
	for (unsigned int k = 0; k < lib_num_code_pages; ++k)
	{
		unsigned int pe = get_free_pe(); // Get PTE id
		allocate_page_user(get_free_page(), pe); // Allocate page
		pte[num_pages + k] = pe;  // Reference PTE
	}
	num_pages += lib_num_code_pages;

	return true;
}

Process* Process::allocate_proc_for_elf_module(GRUB_module* module)
{
	// Allocate process struct
	Process* proc = new Process();
	if (proc == nullptr)
	{
		printf_error("Unable to allocate memory for process");
		return nullptr;
	}
	memset(proc, 0, sizeof(Process));
	proc->elf = new ELF(module);

	unsigned int highest_addr = proc->elf->get_highest_runtime_addr();
	// Num pages code spans over
	unsigned int proc_code_pages = highest_addr / PAGE_SIZE + ((highest_addr % PAGE_SIZE) ? 1 : 0);
	proc->num_pages = proc_code_pages;
	proc->pte = (uint*) malloc(proc->num_pages * sizeof(unsigned int));
	proc->allocs = new list();
	if (proc->pte == nullptr)
	{
		printf_error("Unable to allocate memory for process");
		return nullptr;
	}
	proc->pte[0] = 0;

	// Allocate process code pages
	for (unsigned int k = 1; k < proc_code_pages; ++k) // Do not map first page, it is unmap for null dereferences
	{
		unsigned int pe = get_free_pe(); // Get PTE id
		proc->pte[k] = pe;  // Reference PTE
		allocate_page_user(get_free_page(), pe); // Allocate page
	}

	return proc;
}

Process* Process::from_ELF(GRUB_module* module)
{
	if (!ELF::is_valid(module, Executable))
		return nullptr;

	Process* proc = allocate_proc_for_elf_module(module);
	if (proc == nullptr)
		return nullptr;

	proc->elf->load_into_process_address_space(proc->pte);

	char* interpereter_name = proc->elf->get_interpreter_name();
	unsigned int dynamic = interpereter_name != nullptr;

	if (dynamic)
	{
		if (proc->dynamic_loading())
			return proc;
		delete proc;
		return nullptr;
	}
	return proc;
}

pid_t Process::get_pid() const
{
	return pid;
}

void Process::set_flag(uint flag)
{
	flags |= flag;
}

bool Process::is_terminated() const
{
	return flags & P_TERMINATED;
}

bool Process::is_waiting_key() const
{
	return flags & P_WAITING_KEY;
}

size_t Process::write_args_to_stack(size_t stack_top_v_addr, int argc, const char** argv)
{
	char* stack_top = (char*) stack_top_v_addr;

	size_t args_len = 0; // Actual size of all args combined
	for (int i = 0; i < argc; ++i)
		args_len += strlen(argv[i]) + 1;
	// Word aligned total args size
	size_t args_occupied_space =
			args_len & (sizeof(int) - 1) ? (args_len & ~(sizeof(int) - 1)) + sizeof(int) : args_len;

	char** argv_ptr = (char**) ((uint) (stack_top - args_occupied_space));
	for (int i = argc - 1; i >= 0; --i)
	{
		size_t l = strlen(argv[i]) + 1; // Current arg len
		stack_top -= l; // Point to beginning of string
		argv_ptr -= 1; // Point to argv[i] location
		*argv_ptr = (char*) (0xC0000000 - ((char*) stack_top_v_addr - stack_top)); // Write argv[i]
		memcpy(stack_top, argv[i], l); // Write ith string
	}

	// Write argc and argv
	char* argv0_addr = (char*) 0xC0000000 - ((char*) stack_top_v_addr - (char*) argv_ptr);
	argv_ptr = (char**) (((uint) argv_ptr - 0x10) & ~0xF); // ABI 16 bytes alignment
	*(argv_ptr + 1) = argv0_addr; // Argv
	*(uint*) (argv_ptr) = argc; // Argc

	return 0xC0000000 - ((char*) stack_top_v_addr - (char*) argv_ptr);
}

Process* Process::from_module(GRUB_module* module, pid_t pid, pid_t ppid, int argc, const char** argv)
{
	// Check whether file is an elf
	Elf32_Ehdr* elf32Ehdr = (Elf32_Ehdr*) module->start_addr;
	unsigned int elf = elf32Ehdr->e_ident[0] == 0x7F && elf32Ehdr->e_ident[1] == 'E' && elf32Ehdr->e_ident[2] == 'L' &&
					   elf32Ehdr->e_ident[3] == 'F';

	// Load process into memory
	Process* proc = elf ? Process::from_ELF(module) : Process::from_binary(module);

	// Ensure process was loaded successfully
	if (proc == nullptr)
	{
		printf_error("Error while loading program. Aborting");
		return nullptr;
	}
	size_t base_address = proc->elf->base_address();
	if (base_address != PAGE_SIZE)
	{
		printf_error("ELF base address (%zu) != PAGE_SIZE (%u). Not supported yet. Aborting", base_address, PAGE_SIZE);
		delete proc;
		return nullptr;
	}

	// Allocate process stack page
	unsigned int p_stack_pe_id = get_free_pe();
	allocate_page_user(get_free_page(), p_stack_pe_id);
	unsigned int p_stack_top_v_addr = p_stack_pe_id * PAGE_SIZE + PAGE_SIZE;

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
	for (unsigned int i = 1; i < proc->num_pages; ++i) // Start at page 1 as page 0 is unmapped for segfaults
	{
		unsigned int page_pde = i / PDT_ENTRIES;
		unsigned int page_pte = i % PDT_ENTRIES;

		unsigned int process_page_id = proc->pte[i];
		unsigned int pte = PTE(process_page_id);

		if (proc->page_tables[page_pde].entries[page_pte] != 0)
			printf_error("Non empty pt entry");
		proc->page_tables[page_pde].entries[page_pte] = pte;
	}

	// Map process stack at 0xBFFFFFFC = 0xCFFFFFFF - 4 at pde 767 and pte 1023, just below the kernel
	if (proc->page_tables[767].entries[1023] != 0)
		printf_error("Process kernel page entry is not empty");
	proc->page_tables[767].entries[1023] = PTE(p_stack_pe_id);

	// Setup PCB
	proc->pid = pid;
	proc->ppid = ppid;
	proc->priority = 1;
	proc->k_stack_top = VIRT_ADDR(k_stack_pde, k_stack_pte, (PAGE_SIZE - sizeof(int)));
	//proc->stack_state.eip = 0;
	proc->stack_state.ss = 0x20 | 0x03;
	proc->stack_state.cs = 0x18 | 0x03;
	proc->stack_state.esp = proc->write_args_to_stack(p_stack_top_v_addr, argc, argv);
	proc->stack_state.eflags = 0x200;
	proc->stack_state.error_code = 0;
	memset(&proc->cpu_state, 0, sizeof(proc->cpu_state));

	return proc;
}
