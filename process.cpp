#include "process.h"

#include "dentry.h"
#include "clib/string.h"
#include "clib/stdio.h"
#include "memory.h"
#include "VFS.h"

/** Change current pdt */
extern "C" void change_pdt_asm_(uint pdt_phys_addr);

Process* Process::from_binary(GRUB_module* module)
{
	uint mod_size = module->size;
	uint mod_code_pages = mod_size / PAGE_SIZE + ((mod_size % PAGE_SIZE) ? 1 : 0); // Num pages code spans over

	// Allocate process memory
	Process* proc = new Process();
	proc->stack_state.eip = 0;
	proc->num_pages = mod_code_pages;
	proc->pte = (uint*) malloc(mod_code_pages * sizeof(uint));

	// Allocate process code pages
	for (uint k = 0; k < mod_code_pages; ++k)
	{
		uint pe = get_free_pe();
		proc->pte[k] = pe;
		allocate_page_user(get_free_page(), pe);
	}


	uint proc_pe_id = 0;
	uint sys_pe_id = proc->pte[proc_pe_id];
	uint code_page_start_addr = module->start_addr + proc_pe_id * PAGE_SIZE;
	uint dest_page_start_addr =
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
	for (uint i = 0; i < num_pages; ++i)
	{
		if (!pte[i])
			continue;
		free_page(pte[i] / PDT_ENTRIES, pte[i] % PDT_ENTRIES);
	}

	// Free process fields
	delete pte;
	delete elf;
	delete kapi_elf;
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

void* Process::operator new[]([[maybe_unused]] size_t size)
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

bool Process::dynamic_loading(uint start_address)
{
	const char* interpereter_name = elf->interpreter_name;
	if (strcmp(interpereter_name, OS_INTERPR) != 0)
	{
		printf_error("Unsupported interpreter: %s", interpereter_name);
		return false;
	}

	Elf32_Dyn* dyn_table = elf->dyn_table;
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
	uint lib_name_idx = 0; // lib name string table index
	char* dyn_str_table = nullptr; // string table
	void* dynsymtab = nullptr; // Dynamic symbols table
	uint dynsyntab_num_entries = 0; // Dynamic symbols table number of entries
	uint dynsymtab_ent_size = 0; // Dynamic symbols table entry size
	uint* file_got_addr = nullptr;
	// Address of GOT within the ELF file (not where it is loaded nor its runtime address)
	// In order to know whether base address should be retrieved from values, refer to Figure 2-10: Dynamic Array Tags
	// in http://www.skyfree.org/linux/references/ELF_Format.pdf
	int c = 0;
	for (Elf32_Dyn* d = dyn_table; d->d_tag != DT_NULL; d++, c++)
	{
		switch (d->d_tag)
		{
			case DT_STRTAB:
				dyn_str_table = (char*) (start_address + d->d_un.d_ptr - base_addr);
				break;
			case DT_SYMTAB:
				dynsymtab = (void*) (start_address + d->d_un.d_ptr - base_addr);
				break;
			case DT_NEEDED:
				lib_name_idx = d->d_un.d_val;
				break;
			case DT_PLTGOT:
				file_got_addr = (uint*) (start_address + d->d_un.d_val - base_addr);
				break;
			case DT_HASH:
				dynsyntab_num_entries = *(((uint*) (start_address + d->d_un.d_val - base_addr)) +
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
		uint type = ELF32_ST_TYPE(dd->st_info);
		if (type != STT_FUNC)
		{
			printf_error("Unsupported symbol type: %u", type);
			return false;
		}
	}

	Elf32_Phdr* got_segment_hdr = elf->get_GOT_segment(file_got_addr, start_address);
	if (got_segment_hdr == nullptr)
		return false;

	// Compute GOT runtime address and loaded GOT address
	uint got_runtime_addr =
			(uint) file_got_addr - (start_address + got_segment_hdr->p_offset) +
			got_segment_hdr->p_vaddr;
	uint got_sys_pe_id = pte[got_runtime_addr / PAGE_SIZE];
	uint* got_addr = (uint*) VIRT_ADDR(got_sys_pe_id / PDT_ENTRIES, got_sys_pe_id % PT_ENTRIES,
	                                   got_runtime_addr % PAGE_SIZE);

	// Load kapi
	if (!((kapi_elf = load_lib("/bin/libkapi.so"))))
		return false;

	// Load libdynlk
	uint proc_num_pages = num_pages;
	ELF* libdynlk_elf;
	if (!((libdynlk_elf = load_lib("/bin/libdynlk.so"))))
		return false;

	// Get libdynlk entry point
	void* libdynlk_entry_point = libdynlk_elf->get_libdynlk_main_runtime_addr(proc_num_pages);
	if (libdynlk_entry_point == nullptr)
	{
		printf_error("Error while loading libdynlk");
		return false;
	}

	// GOT setup: GOT[0] unused, GOT[1] = addr of GRUB module (to identify the program), GOT[2] = dynamic linker address
	*(void**) (got_addr + 1) = (void*) this;
	*(void**) (got_addr + 2) = libdynlk_entry_point;

	// Indicate entry point
	stack_state.eip = elf->global_hdr.e_entry;

	return true;
}

ELF* Process::load_lib(const char* path)
{
	void* buf = VFS::load_file(path);
	if (!buf)
		return nullptr;
	uint start_address = (uint) buf;

	if (!ELF::is_valid(start_address, SharedObject))
		return nullptr;

	ELF* lib_elf = new ELF(start_address);
	delete[] (char*) buf;

	// Allocate space for lib int process' address space
	if (!alloc_and_add_lib_pages_to_process(*lib_elf))
	{
		delete lib_elf;
		return nullptr;
	}
	// Load lib into newly allocated space
	load_elf(lib_elf, start_address);

	return lib_elf;
}

bool Process::alloc_and_add_lib_pages_to_process(ELF& lib_elf)
{
	uint highest_addr = lib_elf.get_highest_runtime_addr();
	// Num pages lib  code spans over
	uint lib_num_code_pages = highest_addr / PAGE_SIZE + ((highest_addr % PAGE_SIZE) ? 1 : 0);

	// Adjust proc pte array - add space for lib code
	uint* new_pte = (uint*) calloc((num_pages + lib_num_code_pages), sizeof(uint));
	if (new_pte == nullptr)
	{
		printf_error("Unable to allocate memory for process");
		return false;
	}
	memcpy(new_pte, pte, num_pages * sizeof(uint));
	delete pte;
	pte = new_pte;

	// Allocate lib code pages
	for (int i = 0; i < lib_elf.global_hdr.e_phnum; ++i)
	{
		Elf32_Phdr* h = &lib_elf.prog_hdrs[i];
		if (h->p_type != PT_LOAD)
			continue;

		uint num_segment_pages = (h->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
		for (size_t j = 0; j < num_segment_pages; ++j)
		{
			uint pte_id = num_pages + h->p_vaddr / PAGE_SIZE + j;
			if (pte[pte_id])
				continue;
			uint pe = get_free_pe(); // Get PTE id
			pte[pte_id] = pe; // Reference PTE
			allocate_page_user(get_free_page(), pe); // Allocate page
		}
	}
	num_pages += lib_num_code_pages;

	return true;
}

Process* Process::allocate_proc_for_elf_module(uint start_address)
{
	// Allocate process struct
	Process* proc = new Process();
	if (proc == nullptr)
	{
		printf_error("Unable to allocate memory for process");
		return nullptr;
	}
	memset(proc, 0, sizeof(Process));
	proc->elf = new ELF(start_address);

	uint highest_addr = proc->elf->get_highest_runtime_addr();
	// Num pages code spans over
	uint proc_code_pages = highest_addr / PAGE_SIZE + ((highest_addr % PAGE_SIZE) ? 1 : 0);
	proc->num_pages = proc_code_pages;
	proc->pte = (uint*) calloc(proc->num_pages, sizeof(uint));
	proc->allocs = new list();
	if (proc->pte == nullptr)
	{
		printf_error("Unable to allocate memory for process");
		return nullptr;
	}

	// Allocate process code pages
	for (int i = 0; i < proc->elf->global_hdr.e_phnum; ++i)
	{
		Elf32_Phdr* h = &proc->elf->prog_hdrs[i];
		if (h->p_type != PT_LOAD)
			continue;

		uint num_segment_pages = (h->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE;
		for (size_t j = 0; j < num_segment_pages; ++j)
		{
			uint pte_id = h->p_vaddr / PAGE_SIZE + j;
			if (proc->pte[pte_id])
				continue;
			uint pe = get_free_pe(); // Get PTE id
			proc->pte[pte_id] = pe; // Reference PTE
			allocate_page_user(get_free_page(), pe); // Allocate page
		}
	}

	return proc;
}

Process* Process::from_ELF(uint start_address)
{
	if (!ELF::is_valid(start_address, Executable))
		return nullptr;

	Process* proc = allocate_proc_for_elf_module(start_address);
	if (proc == nullptr)
		return nullptr;

	proc->load_elf(proc->elf, start_address);

	const char* interpereter_name = proc->elf->interpreter_name;
	bool dynamic = interpereter_name != nullptr;

	if (dynamic)
	{
		if (proc->dynamic_loading(start_address))
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

Process* Process::from_memory(uint start_addr, pid_t pid, pid_t ppid, int argc, const char** argv)
{
	// Check whether file is an elf
	Elf32_Ehdr* elf32Ehdr = (Elf32_Ehdr*) start_addr;
	bool elf = elf32Ehdr->e_ident[0] == 0x7F && elf32Ehdr->e_ident[1] == 'E' && elf32Ehdr->e_ident[2] == 'L' &&
	           elf32Ehdr->e_ident[3] == 'F';
	if (!elf)
	{
		printf_error("Unable to find ELF header");
		return nullptr;
	}

	// Load process into memory
	Process* proc = /*elf ? */from_ELF(start_addr)/* : from_binary(start_addr)*/;

	// Ensure process was loaded successfully
	if (proc == nullptr)
	{
		printf_error("Error while loading program. Aborting");
		return nullptr;
	}

	// Allocate process stack page
	uint p_stack_pe_id = get_free_pe();
	allocate_page_user(get_free_page(), p_stack_pe_id);
	uint p_stack_top_v_addr = p_stack_pe_id * PAGE_SIZE + PAGE_SIZE;

	// Allocate syscall handler stack page
	uint k_stack_pe = get_free_pe();
	uint k_stack_pde = k_stack_pe / PDT_ENTRIES;
	uint k_stack_pte = k_stack_pe % PDT_ENTRIES;
	allocate_page(get_free_page(), k_stack_pe);

	// Set process PDT entries: entries 0 to 767 map the process address space using its own page tables
	// Entries 768 to 1024 point to kernel page tables, so that kernel is mapped. Moreover, syscall handlers
	// will not need to switch to kernel pdt to make changes in kernel memory as it is mapped the same way
	// in every process' PDT
	// Set process pdt entries to target process page tables
	page_table_t* page_tables = get_page_tables();
	for (int i = 0; i < 768; ++i)
		proc->pdt.entries[i] = PHYS_ADDR((uint) &proc->page_tables[i]) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;
	// Use kernel page tables for the rest
	for (int i = 768; i < PDT_ENTRIES; ++i)
		proc->pdt.entries[i] = PHYS_ADDR((uint) &page_tables[i]) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;

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

void
Process::copy_elf_subsegment_to_address_space(void* bytes_ptr, uint n, bool no_write, Elf32_Phdr* h, uint& pte_offset,
                                              uint& copied_bytes, page_table_t* sys_page_tables)
{
	// Compute indexes
	uint* elf_pte = pte + pte_offset;
	uint ppte_id = (h->p_vaddr + copied_bytes) / PAGE_SIZE;
	uint sys_pe_id = elf_pte[ppte_id];
	uint sys_pde_id = sys_pe_id / PDT_ENTRIES;
	uint sys_pte_id = sys_pe_id % PT_ENTRIES;
	uint proc_pde_id = (pte_offset + ppte_id) / PT_ENTRIES;
	uint proc_pte_id = (pte_offset + ppte_id) % PT_ENTRIES;

	// Map code
	page_tables[proc_pde_id].entries[proc_pte_id] = sys_page_tables[sys_pde_id].entries[sys_pte_id];
	if (no_write)
		page_tables[proc_pde_id].entries[proc_pte_id] &= ~PAGE_WRITE;

	// Copy code
	uint dest_page_start_addr = VIRT_ADDR(sys_pe_id / PDT_ENTRIES, sys_pe_id % PDT_ENTRIES, 0);
	memcpy((void*) dest_page_start_addr, bytes_ptr, n);

	// Update counter
	copied_bytes += n;
}

void Process::load_elf(ELF* load_elf, uint elf_start_address)
{
	uint elf_num_pages = load_elf->num_pages();
	uint pte_offset = num_pages - elf_num_pages;
	page_table_t* sys_page_tables = get_page_tables();

	// Copy lib code in allocated space and map it
	for (int k = 0; k < load_elf->global_hdr.e_phnum; ++k)
	{
		Elf32_Phdr* h = &load_elf->prog_hdrs[k];
		if (h->p_type != PT_LOAD)
			continue;

		bool no_write = !(h->p_flags & PF_W); // Segment has no write permissions

		uint copied_bytes = 0;
		void* bytes_ptr = (void*) (elf_start_address + h->p_offset);

		// Copy first bytes if they are not page aligned
		if (h->p_vaddr % PAGE_SIZE)
		{
			uint rem = PAGE_SIZE - h->p_vaddr % PAGE_SIZE;
			uint num_first_bytes = rem > h->p_filesz ? h->p_filesz : rem;
			copy_elf_subsegment_to_address_space(bytes_ptr, num_first_bytes, no_write, h, pte_offset, copied_bytes,
			                                     sys_page_tables);
		}

		// Copy whole pages
		while (copied_bytes + PAGE_SIZE < h->p_filesz)
		{
			bytes_ptr = (void*) (elf_start_address + h->p_offset + copied_bytes);
			copy_elf_subsegment_to_address_space(bytes_ptr, PAGE_SIZE, no_write, h, pte_offset, copied_bytes,
			                                     sys_page_tables);
		}

		// Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
		uint num_final_bytes = h->p_filesz - copied_bytes;
		if (!num_final_bytes)
			continue;
		bytes_ptr = (void*) (elf_start_address + h->p_offset + copied_bytes);
		copy_elf_subsegment_to_address_space(bytes_ptr, num_final_bytes, no_write, h, pte_offset, copied_bytes,
		                                     sys_page_tables);
	}
}
