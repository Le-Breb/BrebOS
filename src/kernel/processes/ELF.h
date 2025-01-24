#ifndef INCLUDE_ELF_H
#define INCLUDE_ELF_H

#include "ELF_defines.h"
#include "kstddef.h"

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
	Elf32_Rel* relocs;
	Elf32_Shdr* dynsym_hdr;
	const char* interpreter_name;
	const char* dynsym_strtab;
	size_t num_relocs;


	explicit ELF(uint start_address);

	~ELF();

	/**
	 * Get the highest address in the runtime address space of an ELF file
	 * @return highest runtime address
	 */
	uint get_highest_runtime_addr() const;

	/**
	 * Get a symbol of an ELF file
	 * @return symbol, NULL if error occurred
	 */
	Elf32_Sym* get_symbol(const char* symbol_name);

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

#endif //INCLUDE_OS_ELF_H
