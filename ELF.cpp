#include "ELF.h"
#include "clib/stdio.h"
#include "clib/string.h"

bool ELF::is_valid(GRUB_module* mod, enum ELF_type expected_type)
{
	ELF elf(mod);

	// Ensure this is an ELF
	if (elf.elf32Ehdr->e_ident[0] != 0x7F || elf.elf32Ehdr->e_ident[1] != 'E' || elf.elf32Ehdr->e_ident[2] != 'L' ||
		elf.elf32Ehdr->e_ident[3] != 'F')
	{
		printf_error("Not an ELF\n");
		return false;
	}

	// Ensure program is 32 bits
	if (elf.elf32Ehdr->e_ident[4] != 1)
	{
		printf_error("Not a 32 bit program");
		return false;
	}

	switch (expected_type)
	{
		case Executable:
			if (elf.elf32Ehdr->e_type != ET_EXEC)
			{
				printf_error("Not an executable ELF");
				return false;
			}
			break;
		case SharedObject:
			if (elf.elf32Ehdr->e_type != ET_DYN)
			{
				printf_error("Not an executable ELF");
				return false;
			}
			break;
		case Relocatable:
			if (elf.elf32Ehdr->e_type != ET_REL)
			{
				printf_error("Not an executable ELF");
				return false;
			}
			break;
	}
	//printf("Endianness: %s\n", elf32Ehdr->e_ident[5] == 1 ? "Little" : "Big");

	// ELF imposes that first section header is null
	if (elf.elf32Shdr->sh_type != SHT_NULL)
	{
		printf_error("Not a valid ELF file");
		return false;
	}

	// Ensure program is supported
	for (int k = 0; k < elf.elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((uint) elf.elf32Shdr + elf.elf32Ehdr->e_shentsize * k);
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
				return false;
		}
	}

	if (elf.elf32Ehdr->e_phnum == 0)
	{
		printf_error("No program header");
		return false;
	}

	// Check that no read-only page overlaps with a write-allowed page
	uint elf_num_pages = elf.num_pages();
	bool* ro_pages = (bool*) calloc(elf_num_pages, sizeof(bool));
	for (int k = 0; k < elf.elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((uint) elf.elf32Phdr + elf.elf32Ehdr->e_phentsize * k);
		if (h->p_type != PT_LOAD)
			continue;

		bool segment_write = h->p_flags & PF_W;

		for (size_t j = 0; j < (h->p_memsz + PAGE_SIZE - 1) / PAGE_SIZE; ++j)
		{
			uint page_id = h->p_vaddr / PAGE_SIZE + j;
			if (segment_write)
			{
				if (ro_pages[page_id])
				{
					free(ro_pages);
					return false;
				}
			}
			else
				ro_pages[page_id] = true;
		}
	}
	free(ro_pages);

	return true;
}

//Todo: Represent the whole PT_LOAD layout to accurately compute the size of the loaded parts
uint ELF::get_highest_runtime_addr()
{
	uint highest_addr = 0;
	for (int k = 0; k < elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((uint) elf32Phdr + elf32Ehdr->e_phentsize * k);
		if (h->p_type != PT_LOAD)
			continue;

		uint high_addr = h->p_vaddr + h->p_memsz;
		if (high_addr > highest_addr)
			highest_addr = high_addr;
	}

	return highest_addr;
}

char* ELF::get_interpreter_name()
{
	for (int k = 0; k < elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((uint) elf32Phdr + elf32Ehdr->e_phentsize * k);

		if (h->p_type != PT_INTERP)
			continue;

		char* interpreter_path = (char*) mod->start_addr + h->p_offset;
		if (strcmp(interpreter_path, OS_INTERPR) != 0)
		{
			printf_error("Unsupported interpreter: %s", interpreter_path);
			return nullptr;
		}
		return interpreter_path;
	}

	return nullptr;
}

void* ELF::get_libdynlk_main_runtime_addr(uint proc_num_pages)
{
	// Find and return main's address
	Elf32_Sym* s = get_symbol("lib_main");

	return s == nullptr ? (void*) s : (void*) (proc_num_pages * PAGE_SIZE + s->st_value);
}

Elf32_Phdr* ELF::get_GOT_segment(const uint* file_got_addr)
{
	Elf32_Phdr* got_segment_hdr = nullptr;
	for (int k = 0; k < elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((uint) elf32Phdr + elf32Ehdr->e_phentsize * k);
		if (h->p_type != PT_LOAD)
			continue;
		if (h->p_offset <= (uint) file_got_addr - mod->start_addr &&
			h->p_offset + h->p_memsz >= (uint) file_got_addr - mod->start_addr)
		{
			got_segment_hdr = h;
			break;
		}
	}
	if (got_segment_hdr == nullptr)
	{
		printf_error("Unable to compute GOT runtime address");
		return nullptr;
	}

	return got_segment_hdr;
}

Elf32_Dyn* ELF::get_dyn_table()
{
	for (int k = 0; k < elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((uint) elf32Shdr + elf32Ehdr->e_shentsize * k);

		if (h->sh_type != SHT_DYNAMIC)
			continue;
		return (Elf32_Dyn*) (mod->start_addr + h->sh_offset);
	}

	return nullptr;
}

Elf32_Rel* ELF::get_rel_plt()
{
	Elf32_Shdr* sh_strtab_h = (Elf32_Shdr*) ((uint) elf32Shdr +
											 elf32Ehdr->e_shentsize * elf32Ehdr->e_shstrndx);
	char* strtab = (char*) (mod->start_addr + sh_strtab_h->sh_offset);
	for (int k = 0; k < elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((uint) elf32Shdr + elf32Ehdr->e_shentsize * k);
		if (h->sh_type != SHT_REL || strcmp(".rel.plt", &strtab[h->sh_name]) != 0)
			continue;
		return (Elf32_Rel*) (mod->start_addr + h->sh_offset);
	}

	return nullptr;
}

Elf32_Shdr* ELF::get_dynsym_hdr()
{
	for (int k = 0; k < elf32Ehdr->e_shnum; ++k)
	{
		Elf32_Shdr* h = (Elf32_Shdr*) ((uint) elf32Shdr + elf32Ehdr->e_shentsize * k);

		if (h->sh_type != SHT_DYNSYM)
			continue;
		return h;
	}

	return nullptr;
}

Elf32_Sym* ELF::get_symbol(const char* symbol_name)
{
	Elf32_Shdr* lib_dynsym_hdr = get_dynsym_hdr();
	char* strtab = (char*) (mod->start_addr +
							((Elf32_Shdr*) ((uint) elf32Shdr +
											elf32Ehdr->e_shentsize * lib_dynsym_hdr->sh_link))->sh_offset);
	uint lib_dynsym_num_entries = lib_dynsym_hdr->sh_size / lib_dynsym_hdr->sh_entsize;

	for (uint i = 1; i < lib_dynsym_num_entries; i++)
	{
		Elf32_Sym* lib_symbol_entry = (Elf32_Sym*) (mod->start_addr + lib_dynsym_hdr->sh_offset +
													i * lib_dynsym_hdr->sh_entsize);
		if (strcmp(&strtab[lib_symbol_entry->st_name], symbol_name) == 0)
			return lib_symbol_entry;
	}

	return nullptr;


}

ELF::ELF(Elf32_Ehdr* elf32Ehdr, Elf32_Phdr* elf32Phdr, Elf32_Shdr* elf32Shdr, GRUB_module* mod) : elf32Ehdr(elf32Ehdr),
																								  elf32Phdr(elf32Phdr),
																								  elf32Shdr(elf32Shdr),
																								  mod(mod)
{}

ELF::ELF(GRUB_module* mod) : ELF((Elf32_Ehdr*) mod->start_addr,
								 (Elf32_Phdr*) (mod->start_addr + ((Elf32_Ehdr*) mod->start_addr)->e_phoff),
								 (Elf32_Shdr*) (mod->start_addr + ((Elf32_Ehdr*) mod->start_addr)->e_shoff), mod)
{
}

size_t ELF::base_address()
{
	size_t lowest_address = -1;
	for (int k = 0; k < elf32Ehdr->e_phnum; ++k)
	{
		Elf32_Phdr* h = (Elf32_Phdr*) ((uint) elf32Phdr + elf32Ehdr->e_phentsize * k);
		if (h->p_type != PT_LOAD)
			continue;

		uint addr = h->p_vaddr;
		if (addr < lowest_address)
			lowest_address = addr;
	}

	return lowest_address & ~(PAGE_SIZE - 1); // Truncate to nearest page size multiple
}

size_t ELF::num_pages()
{
	return (get_highest_runtime_addr() + PAGE_SIZE - 1) / PAGE_SIZE;
}
