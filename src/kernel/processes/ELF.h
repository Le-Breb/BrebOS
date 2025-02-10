#ifndef INCLUDE_ELF_H
#define INCLUDE_ELF_H

#include "ELF_defines.h"
#include <kstddef.h>
#include <kstring.h>

#define OS_INTERPR ("/dynlk") // OS default interpreter, used to run dynamically linked programs
#define OS_LIB ("libc.so") // OS lib, allowing programs to use syscalls

enum ELF_type
{
	Executable,
	SharedObject,
	Relocatable
};

class ELF
{
private:
	ELF(Elf32_Ehdr* elf32Ehdr, Elf32_Phdr* elf32Phdr, Elf32_Shdr* elf32Shdr, uint start_address);

public:
	Elf32_Ehdr global_hdr;
	Elf32_Phdr* prog_hdrs;
	Elf32_Shdr* section_hdrs;
	Elf32_Dyn* dyn_table;
	Elf32_Sym* symbols;
	Elf32_Rel* plt_relocs;
	Elf32_Rel* dyn_relocs;
	Elf32_Shdr* dynsym_hdr;
	const char* interpreter_name;
	const char* dynsym_strtab;
	size_t num_plt_relocs;
	size_t num_dyn_relocs;

	explicit ELF(uint start_address);

	~ELF();

	/**
	 * Get the highest address in the runtime address space of an ELF file
	 * @return highest runtime address
	 */
	[[nodiscard]] uint get_highest_runtime_addr() const;

	/**
	 * Get a symbol of an ELF file
	 * @return symbol, NULL if error occurred
	 */
	Elf32_Sym* get_symbol(const char* symbol_name) const;

	/**
	 * Checks whether an ELF file is valid and supported
	 * @param start_address GRUB module containing the ELF
	 * @param expected_type expected type of the ELF
	 * @return ELF* if it is valid, nullptr otherwise
	 */
	static ELF* is_valid(uint start_address, ELF_type expected_type);

	/**
	 * Get entry point of libdynlk in a process' runtime address space
	 * @param proc_num_pages number of pages the process (without libdynl) spans over
	 * @return libdynlk runtime address, NULL if an error occured
	 */
	void* get_libdynlk_main_runtime_addr(uint proc_num_pages);


	/**
	 * Get segment in which the GOT lays
	 * @param file_got_addr GOT address (in the ELF)
	 * @param start_address
	 * @return GOT segment header, NULL if an error occurred
	 */
	Elf32_Phdr* get_GOT_segment(const uint* file_got_addr, uint start_address) const;

	[[nodiscard]] size_t base_address() const;

	[[nodiscard]] size_t num_pages() const;
};

struct elf_dependence_list
{
	const char* path; // Path of the ELF, owned by this structure
	ELF* elf;
	elf_dependence_list* next;
	Elf32_Addr runtime_load_address;

	elf_dependence_list(const char* path, ELF* elf, Elf32_Addr runtime_load_address)
		: path((char*)malloc(strlen(path) + 1)),
		  elf(elf),
		  next(nullptr),
		  runtime_load_address(runtime_load_address)
	{
		strcpy((char*)this->path, path);
	}

	void add_dependence(const char* path, ELF* elf, Elf32_Addr runtime_load_address)
	{
		elf_dependence_list* curr = this;
		while (curr->next)
			curr = curr->next;
		curr->next = new elf_dependence_list(path, elf, runtime_load_address);
	}

	~elf_dependence_list()
	{
		delete path;
		delete next;
		delete elf;
	}
};

#endif //INCLUDE_ELF_H
