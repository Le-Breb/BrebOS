#include "elf_tools.h"
#include "clib/stdio.h"
#include "clib/string.h"

enum ELF_type
{
	Executable,
	SharedObject,
	Relocatable
};

/**
 * Checks whether an ELF file is valid and supported
 * @param mod GRUB module containing the ELF
 * @param expected_type expected type of the ELF
 * @return boolean indicating whether the file is valid or not
 */
unsigned int valid_elf(GRUB_module* mod, enum ELF_type expected_type);

/**
 * Get the interpreter name of an ELF file
 * @param mod_start_addr
 * @param elf32Ehdr ELF header
 * @param elf32Phdr program header table
 * @return name of the interpreter, NULL if the ELF doesn't use one
 */
char* get_elf_interpreter_name(elf_info* elfi);

/**
 * Load ELF file code and data into already allocated pte
 * @param mod GRUB module containing the ELF
 * @param elf32Ehdr ELF header
 * @param elf32Phdr program header table
 * @param proc_pte page table entry array, ie where to load stuff
 */
void load_elf_program(elf_info* elfi, const unsigned int* proc_pte);

/**
 * Allocate a process struct
 * @param highest_addr ELF highest runtime address
 * @return process struct, NULL if an error occurred
 */
process* allocate_process(unsigned int highest_addr);

/**
 * Get entry point of libdynlk in a process' runtime address space
 * @param elfi GRUB module containing libdynlk
 * @param elf32Ehdr ELF header
 * @param elf32Shdr section header table
 * @param proc_num_pages number of pages the process (without libdynl) spans over
 * @return libdynlk runtime address, NULL if an error occured
 */
void* get_libdynlk_main_runtime_addr(elf_info* elfi, unsigned int proc_num_pages);

/**
 * Allocate space for libydnlk and add it to a process' address space
 * @param elf32Ehdr ELF heder
 * @param elf32Phdr program table header
 * @param p process to add libdynlk to
 * @return boolean indicating success state
 */
unsigned int alloc_and_add_lib_pages_to_process(process* p, elf_info* lib_elfi);

/**
 * Load libdynlk in a process' address soace
 * @param p process to add libynlk to
 * @param lib_mod GRUB modules
 * @return libdynlk runtime entry point address
 */
unsigned int load_lib(process* p, GRUB_module* lib_mod);

/**
 * Get segment in which the GOT lays
 * @param mod GRUB module containing the ELF
 * @param elf32Ehdr ELF header
 * @param elf32Phdr program table header
 * @param file_got_addr GOT address (in the ELF)
 * @return GOT segment header, NULL if an error occurred
 */
Elf32_Phdr*
get_elf_GOT_segment(elf_info* elfi, const unsigned int* file_got_addr);

/**
 * Sets up a dynamically linked process
 * @param proc process
 * @param grub_modules grub modules
 * @return process set up, NULL if an error occurred
 */
process* dynamic_loader(process* proc, GRUB_module* grub_modules);

unsigned int valid_elf(GRUB_module* mod, enum ELF_type expected_type)
{
	Elf32_Ehdr* elf32Ehdr = (Elf32_Ehdr*) mod->start_addr;

	// Ensure this is an ELF
	if (elf32Ehdr->e_ident[0] != 0x7F || elf32Ehdr->e_ident[1] != 'E' || elf32Ehdr->e_ident[2] != 'L' ||
		elf32Ehdr->e_ident[3] != 'F')
	{
		printf_error("Not an ELF\n");
		return 0x00;
	}

	// Ensure program is 32 bits
	if (elf32Ehdr->e_ident[4] != 1)
	{
		printf_error("Not a 32 bit program");
		return 0x00;
	}

	switch (expected_type)
	{
		case Executable:
			if (elf32Ehdr->e_type != ET_EXEC)
			{
				printf_error("Not an executable ELF");
				return 0x00;
			}
			break;
		case SharedObject:
			if (elf32Ehdr->e_type != ET_DYN)
			{
				printf_error("Not an executable ELF");
				return 0x00;
			}
			break;
		case Relocatable:
			if (elf32Ehdr->e_type != ET_REL)
			{
				printf_error("Not an executable ELF");
				return 0x00;
			}
			break;
	}
	//printf("Endianness: %s\n", elf32Ehdr->e_ident[5] == 1 ? "Little" : "Big");

	// Get section headers
	Elf32_Shdr* elf32Shdr = (Elf32_Shdr*) (mod->start_addr + elf32Ehdr->e_shoff);

	// ELF imposes that first section header is null
	if (elf32Shdr->sh_type != SHT_NULL)
	{
		printf_error("Not a valid ELF file");
		return 0x00;
	}

	// Ensure program is supported
	for (int k = 0; k < elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((unsigned int) elf32Shdr + elf32Ehdr->e_shentsize * k);
		switch (h->sh_type)
		{
			case SHT_NULL:
			case SHT_PROGBITS:
			case SHT_NOBITS:
			case SHT_SYMTAB:
			case SHT_STRTAB:
			case SHT_DYNSYM:
			case SHT_HASH:
			case SHT_GNU_HASH:
			case SHT_REL:
			case SHT_DYNAMIC:
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

	return 0x01;
}

//Todo: Represent the whole PT_LOAD layout to accurately compute the size of the loaded parts
unsigned int get_elf_highest_runtime_addr(elf_info* elfi)
{
	unsigned int highest_addr = 0;
	for (int k = 0; k < elfi->elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((unsigned int) elfi->elf32Phdr + elfi->elf32Ehdr->e_phentsize * k);
		if (h->p_type != PT_LOAD)
			continue;

		unsigned int high_addr = h->p_vaddr + h->p_memsz;
		if (high_addr > highest_addr)
			highest_addr = high_addr;
	}

	return highest_addr;
}

char* get_elf_interpreter_name(elf_info* elfi)
{
	for (int k = 0; k < elfi->elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((unsigned int) elfi->elf32Phdr + elfi->elf32Ehdr->e_phentsize * k);

		if (h->p_type != PT_INTERP)
			continue;

		char* interpreter_path = (char*) elfi->mod->start_addr + h->p_offset;
		if (strcmp(interpreter_path, OS_INTERPR) != 0)
		{
			printf_error("Unsupported interpreter: %s", interpreter_path);
			return 0x00;
		}
		return interpreter_path;
	}

	return 0x00;
}

void load_elf_program(elf_info* elfi, const unsigned int* proc_pte)
{
	// Copy lib code in allocated space
	for (int k = 0; k < elfi->elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((unsigned int) elfi->elf32Phdr + elfi->elf32Ehdr->e_phentsize * k);
		if (h->p_type != PT_LOAD)
			continue;

		unsigned int copied_bytes = 0;
		unsigned int sys_pe_id = proc_pte[h->p_vaddr / PAGE_SIZE];
		unsigned int rem_bytes_addr = elfi->mod->start_addr + h->p_offset;
		unsigned int dest_page_start_addr = VIRT_ADDR(sys_pe_id / PDT_ENTRIES, sys_pe_id % PT_ENTRIES,
													  h->p_vaddr % PAGE_SIZE);

		// Copy first bytes if they are not page aligned
		if (h->p_vaddr % PAGE_SIZE)
		{
			unsigned int rem = PAGE_SIZE - h->p_vaddr % PAGE_SIZE;
			unsigned int first_bytes = h->p_filesz > rem ? rem : h->p_filesz;
			memcpy((void*) dest_page_start_addr, (void*) rem_bytes_addr, first_bytes);
			copied_bytes += first_bytes;
		}

		// Copy whole pages
		while (copied_bytes + PAGE_SIZE < h->p_filesz)
		{
			printf_info("code inside this loop is not guaranteed to work, it hasn\'t been tested yet");
			rem_bytes_addr = elfi->mod->start_addr + h->p_offset + copied_bytes;
			sys_pe_id = proc_pte[(h->p_vaddr + copied_bytes) / PAGE_SIZE];
			dest_page_start_addr = VIRT_ADDR(sys_pe_id / PDT_ENTRIES, sys_pe_id % PDT_ENTRIES, 0);
			memcpy((void*) dest_page_start_addr, (void*) rem_bytes_addr, PAGE_SIZE);
			copied_bytes += PAGE_SIZE;
		}

		// Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
		rem_bytes_addr = elfi->mod->start_addr + h->p_offset + copied_bytes;
		sys_pe_id = proc_pte[(h->p_vaddr + copied_bytes) / PAGE_SIZE];
		dest_page_start_addr = VIRT_ADDR(sys_pe_id / PDT_ENTRIES, sys_pe_id % PDT_ENTRIES, 0);
		unsigned int num_final_bytes = h->p_filesz - copied_bytes;
		memcpy((void*) dest_page_start_addr, (void*) rem_bytes_addr, num_final_bytes);
		copied_bytes += h->p_filesz % PAGE_SIZE;
		if (num_final_bytes)
			memset((void*) (dest_page_start_addr + num_final_bytes), 0, PAGE_SIZE - num_final_bytes);
	}
}

process* allocate_process(unsigned int highest_addr)
{
	// Allocate process struct
	process* proc = page_aligned_malloc(sizeof(process));
	if (proc == 0x00)
	{
		printf_error("Unable to allocate memory for process");
		return 0x00;
	}
	memset(proc, 0, sizeof(process));

	// Num pages code spans over
	unsigned int proc_code_pages = highest_addr / PAGE_SIZE + ((highest_addr % PAGE_SIZE) ? 1 : 0);
	proc->num_pages = proc_code_pages;
	proc->pte = malloc(proc->num_pages * sizeof(unsigned int));
	if (proc->pte == 0x00)
	{
		printf_error("Unable to allocate memory for process");
		return 0x00;
	}

	// Allocate process code pages
	for (unsigned int k = 0; k < proc_code_pages; ++k)
	{
		unsigned int pe = get_free_pe(); // Get PTE id
		proc->pte[k] = pe;  // Reference PTE
		allocate_page_user(get_free_page(), pe); // Allocate page
	}

	proc->elfi = malloc(sizeof(elf_info));

	return proc;
}

void* get_libdynlk_main_runtime_addr(elf_info* elfi, unsigned int proc_num_pages)
{
	// Find and return main's address
	Elf32_Sym* s = get_elf_symbol(elfi, "lib_main");

	return s == 0x00 ? (void*) s : (void*) (proc_num_pages * PAGE_SIZE + s->st_value);
}

unsigned int alloc_and_add_lib_pages_to_process(process* p, elf_info* lib_elfi)
{
	unsigned int highest_addr = get_elf_highest_runtime_addr(lib_elfi);
	// Num pages lib  code spans over
	unsigned int lib_num_code_pages = highest_addr / PAGE_SIZE + ((highest_addr % PAGE_SIZE) ? 1 : 0);

	// Adjust proc pte array - add space for lib code
	unsigned int* new_pte = malloc((p->num_pages + lib_num_code_pages) * sizeof(unsigned int));
	if (new_pte == 0x00)
	{
		printf_error("Unable to allocate memory for process");
		return 0x00;
	}
	memcpy(new_pte, p->pte, p->num_pages * sizeof(unsigned int));
	free(p->pte);
	p->pte = new_pte;

	// Allocate lib code pages
	for (unsigned int k = 0; k < lib_num_code_pages; ++k)
	{
		unsigned int pe = get_free_pe(); // Get PTE id
		allocate_page_user(get_free_page(), pe); // Allocate page
		p->pte[p->num_pages + k] = pe;  // Reference PTE
	}
	p->num_pages += lib_num_code_pages;

	return 0x01;
}

unsigned int load_lib(process* p, GRUB_module* lib_mod)
{
	if (!valid_elf(lib_mod, SharedObject))
		return 0x00;

	unsigned int proc_num_pages = p->num_pages;
	elf_info lib_elfi = {(Elf32_Ehdr*) lib_mod->start_addr,
						 (Elf32_Phdr*) (lib_mod->start_addr + lib_elfi.elf32Ehdr->e_phoff),
						 (Elf32_Shdr*) (lib_mod->start_addr + lib_elfi.elf32Ehdr->e_shoff), lib_mod};

	// Allocate space for lib int process' address space
	if (!alloc_and_add_lib_pages_to_process(p, &lib_elfi))
		return 0x00;
	// Load lib into newly allocated space
	load_elf_program(&lib_elfi, p->pte + proc_num_pages);

	return 0x01;
}

Elf32_Phdr* get_elf_GOT_segment(elf_info* elfi, const unsigned int* file_got_addr)
{
	Elf32_Phdr* got_segment_hdr = 0x00;
	for (int k = 0; k < elfi->elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((unsigned int) elfi->elf32Phdr + elfi->elf32Ehdr->e_phentsize * k);
		if (h->p_type != PT_LOAD)
			continue;
		if (h->p_offset <= (unsigned int) file_got_addr - elfi->mod->start_addr &&
			h->p_offset + h->p_memsz >= (unsigned int) file_got_addr - elfi->mod->start_addr)
		{
			got_segment_hdr = h;
			break;
		}
	}
	if (got_segment_hdr == 0x00)
	{
		printf_error("Unable to compute GOT runtime address");
		return 0x00;
	}

	return got_segment_hdr;
}

Elf32_Dyn* get_elf_dyn_table(elf_info* elfi)
{
	for (int k = 0; k < elfi->elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((unsigned int) elfi->elf32Shdr + elfi->elf32Ehdr->e_shentsize * k);

		if (h->sh_type != SHT_DYNAMIC)
			continue;
		return (Elf32_Dyn*) (elfi->mod->start_addr + h->sh_offset);
	}

	return 0x00;
}

process* dynamic_loader(process* proc, GRUB_module* grub_modules)
{
	elf_info* elfi = proc->elfi;
	char* interpereter_name = get_elf_interpreter_name(elfi);
	if (strcmp(interpereter_name, OS_INTERPR) != 0)
	{
		printf_error("Unsupported interpreter: %s", interpereter_name);
		return 0x00;
	}

	Elf32_Dyn* dyn_table = get_elf_dyn_table(elfi);
	if (dyn_table == 0x00)
		return 0x00;

	// Get dynamic linking info
	unsigned int lib_name_idx = 0; // lib name string table index
	char* dyn_str_table = 0x00; // string table
	void* dynsymtab = 0x00; // Dynamic symbols table
	unsigned int dynsyntab_num_entries; // Dynamic symbols table number of entries
	unsigned int dynsymtab_ent_size; // Dynamic symbols table entry size
	unsigned int* file_got_addr; // Address of GOT within the ELF file (not where it is loaded nor its runtime address)
	for (Elf32_Dyn* d = dyn_table; d->d_tag != DT_NULL; d++)
	{
		switch (d->d_tag)
		{
			case DT_STRTAB:
				dyn_str_table = (char*) (elfi->mod->start_addr + d->d_un.d_ptr);
				break;
			case DT_SYMTAB:
				dynsymtab = (void*) (elfi->mod->start_addr + d->d_un.d_ptr);
				break;
			case DT_NEEDED:
				lib_name_idx = d->d_un.d_val;
				break;
			case DT_PLTGOT:
				file_got_addr = (unsigned int*) (elfi->mod->start_addr + d->d_un.d_val);
				break;
			case DT_HASH:
				dynsyntab_num_entries = *(((unsigned int*) (elfi->mod->start_addr + d->d_un.d_val)) + 1); // nchain
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
	if (dyn_str_table == 0x00)
	{
		printf_error("No dynamic string table");
		return 0x00;
	}
	if (dynsymtab == 0x00)
	{
		printf_error("No dynamic symbol table");
		return 0x00;
	}
	if (file_got_addr == 0x00)
	{
		printf_error("No GOT");
		return 0x00;
	}

	// Get needed lib name and check it is not missing
	char* lib_name = &dyn_str_table[lib_name_idx];
	if (strcmp(lib_name, OS_LIB) != 0)
		printf_error("Missing library: %s\n", lib_name);

	// Ensure dynamic symbols are supported
	for (unsigned int i = 1; i < dynsyntab_num_entries; ++i) // First entry has to be null, we skip it
	{
		Elf32_Sym* dd = (Elf32_Sym*) (dynsymtab + i * dynsymtab_ent_size);
		//printf("%s\n", &dyn_str_table[dd->st_name]);
		unsigned int type = ELF32_ST_TYPE(dd->st_info);
		if (type != STT_FUNC)
		{
			printf_error("Unsupported symbol type: %u", type);
			return 0x00;
		}
	}

	Elf32_Phdr* got_segment_hdr = get_elf_GOT_segment(elfi, file_got_addr);
	if (got_segment_hdr == 0x00)
		return 0x00;

	// Compute GOT runtime address and loaded GOT address
	unsigned int got_runtime_addr =
			(unsigned int) file_got_addr - (elfi->mod->start_addr + got_segment_hdr->p_offset) +
			got_segment_hdr->p_vaddr;
	unsigned int got_sys_pe_id = proc->pte[got_runtime_addr / PAGE_SIZE];
	unsigned int* got_addr = (unsigned int*) VIRT_ADDR(got_sys_pe_id / PDT_ENTRIES, got_sys_pe_id % PT_ENTRIES,
													   got_runtime_addr % PAGE_SIZE);

	// Load klib
	if (!load_lib(proc, &grub_modules[3]))
		return 0x00;
	// Load libdynlk
	unsigned int proc_num_pages = proc->num_pages;
	if (!load_lib(proc, &grub_modules[2]))
		return 0x00;

	// Get libdynlk entry point
	elf_info libdynlk_elfi = {(Elf32_Ehdr*) grub_modules[2].start_addr,
							  (Elf32_Phdr*) (grub_modules[2].start_addr + libdynlk_elfi.elf32Ehdr->e_phoff),
							  (Elf32_Shdr*) (grub_modules[2].start_addr + libdynlk_elfi.elf32Ehdr->e_shoff),
							  &grub_modules[2]};
	void* libdynlk_entry_point = get_libdynlk_main_runtime_addr(&libdynlk_elfi, proc_num_pages);
	if (libdynlk_entry_point == 0x00)
	{
		printf_error("Error while loading libdynlk");
		return 0x00;
	}

	// GOT setup: GOT[0] unused, GOT[1] = addr of GRUB module (to identify the program), GOT[2] = dynamic linker address
	*(void**) (got_addr + 1) = (void*) proc;
	*(void**) (got_addr + 2) = libdynlk_entry_point;

	// Indicate entry point
	proc->stack_state.eip = elfi->elf32Ehdr->e_entry;

	return proc;
}

process* load_elf(unsigned int module, GRUB_module* grub_modules)
{
	if (!valid_elf(&grub_modules[module], Executable))
		return 0x00;

	elf_info elfi = {(Elf32_Ehdr*) grub_modules[module].start_addr,
					 (Elf32_Phdr*) (grub_modules[module].start_addr + elfi.elf32Ehdr->e_phoff),
					 (Elf32_Shdr*) (grub_modules[module].start_addr + elfi.elf32Ehdr->e_shoff),
					 &grub_modules[module]};

	process* proc = allocate_process(get_elf_highest_runtime_addr(&elfi));
	if (proc == 0x00)
		return 0x00;
	memcpy(proc->elfi, &elfi, sizeof(elf_info));

	load_elf_program(&elfi, proc->pte);

	char* interpereter_name = get_elf_interpreter_name(&elfi);
	unsigned int dynamic = interpereter_name != 0x00;

	return dynamic ? dynamic_loader(proc, grub_modules) : proc;
}

Elf32_Rel* get_elf_rel_plt(elf_info* elfi)
{
	Elf32_Shdr* sh_strtab_h = (Elf32_Shdr*) ((unsigned int) elfi->elf32Shdr +
											 elfi->elf32Ehdr->e_shentsize * elfi->elf32Ehdr->e_shstrndx);
	char* strtab = (char*) (elfi->mod->start_addr + sh_strtab_h->sh_offset);
	for (int k = 0; k < elfi->elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((unsigned int) elfi->elf32Shdr + elfi->elf32Ehdr->e_shentsize * k);
		if (h->sh_type != SHT_REL || strcmp(".rel.plt", &strtab[h->sh_name]) != 0)
			continue;
		return (Elf32_Rel*) (elfi->mod->start_addr + h->sh_offset);
	}

	return 0x00;
}

Elf32_Shdr* get_elf_dynsym_hdr(elf_info* elfi)
{
	for (int k = 0; k < elfi->elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((unsigned int) elfi->elf32Shdr + elfi->elf32Ehdr->e_shentsize * k);

		if (h->sh_type != SHT_DYNSYM)
			continue;
		return h;
	}

	return 0x00;
}

Elf32_Sym* get_elf_symbol(elf_info* elfi, char* symbol_name)
{
	Elf32_Shdr* lib_dynsym_hdr = get_elf_dynsym_hdr(elfi);
	char* strtab = (char*) (elfi->mod->start_addr +
							((Elf32_Shdr*) ((unsigned int) elfi->elf32Shdr +
											elfi->elf32Ehdr->e_shentsize * lib_dynsym_hdr->sh_link))->sh_offset);
	unsigned int lib_dynsym_num_entries = lib_dynsym_hdr->sh_size / lib_dynsym_hdr->sh_entsize;

	for (unsigned int i = 1; i < lib_dynsym_num_entries; i++)
	{
		Elf32_Sym* lib_symbol_entry = (Elf32_Sym*) (elfi->mod->start_addr + lib_dynsym_hdr->sh_offset +
													i * lib_dynsym_hdr->sh_entsize);
		if (strcmp(&strtab[lib_symbol_entry->st_name], symbol_name) == 0)
			return lib_symbol_entry;
	}

	return 0x00;
}
