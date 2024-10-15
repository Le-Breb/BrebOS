#include "process.h"
#include "clib/string.h"
#include "clib/stdio.h"
#include "memory.h"
#include "PIT.h"

/** Change current pdt */
extern "C" void change_pdt_asm_(unsigned int pdt_phys_addr);

Process* Process::from_binary(unsigned int module, GRUB_module* grub_modules)
{
	unsigned int mod_size = grub_modules[module].size;
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

Process::~Process()
{
	if (!(flags & P_TERMINATED))
	{
		printf_error("Process %u is not terminated", pid);
		return;
	}

	// Free process code
	for (unsigned int i = 0; i < num_pages; ++i)
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

	// Get dynamic linking info
	unsigned int lib_name_idx = 0; // lib name string table index
	char* dyn_str_table = nullptr; // string table
	void* dynsymtab = nullptr; // Dynamic symbols table
	unsigned int dynsyntab_num_entries; // Dynamic symbols table number of entries
	unsigned int dynsymtab_ent_size; // Dynamic symbols table entry size
	unsigned int* file_got_addr; // Address of GOT within the ELF file (not where it is loaded nor its runtime address)
	for (Elf32_Dyn* d = dyn_table; d->d_tag != DT_NULL; d++)
	{
		switch (d->d_tag)
		{
			case DT_STRTAB:
				dyn_str_table = (char*) (elf->mod->start_addr + d->d_un.d_ptr);
				break;
			case DT_SYMTAB:
				dynsymtab = (void*) (elf->mod->start_addr + d->d_un.d_ptr);
				break;
			case DT_NEEDED:
				lib_name_idx = d->d_un.d_val;
				break;
			case DT_PLTGOT:
				file_got_addr = (unsigned int*) (elf->mod->start_addr + d->d_un.d_val);
				break;
			case DT_HASH:
				dynsyntab_num_entries = *(((unsigned int*) (elf->mod->start_addr + d->d_un.d_val)) + 1); // nchain
				break;
			case DT_GNU_HASH:
				printf("GNU hash\n");
				break;
			case DT_STRSZ:
				printf("string table size\n");
				break;
			case DT_SYMENT:
				dynsymtab_ent_size = d->d_un.d_val;
				break;
			case DT_DEBUG:
				printf("debug stuff\n");
				break;
			case DT_PLTRELSZ:
				printf("size of PLT relocs (\?\?)\n");
				break;
			case DT_PLTREL:
				printf("PLT reloc type: %s (\?\?)\n", d->d_un.d_val == DT_REL ? "DT_REL" : "DT_RELA");
				break;
			case DT_JMPREL:
				printf("address of PLT relocs (\?\?)\n");
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
	for (unsigned int i = 1; i < dynsyntab_num_entries; ++i) // First entry has to be null, we skip it
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

	// Allocate process code pages
	for (unsigned int k = 0; k < proc_code_pages; ++k)
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
