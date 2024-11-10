#ifndef INCLUDE_ELF_H
#define INCLUDE_ELF_H

#include "ELF_defines.h"
#include "clib/stddef.h"

#define OS_INTERPR ("/dynlk") // OS default interpreter, used to run dynamically linked programs
#define OS_LIB ("libsyscalls.so") // OS lib, allowing programs to use syscalls

enum ELF_type
{
	Executable,
	SharedObject,
	Relocatable
};

class ELF
{
private:
	ELF(Elf32_Ehdr* elf32Ehdr, Elf32_Phdr* elf32Phdr, Elf32_Shdr* elf32Shdr, GRUB_module* mod);

public:

	Elf32_Ehdr* elf32Ehdr;
	Elf32_Phdr* elf32Phdr;
	Elf32_Shdr* elf32Shdr;
	GRUB_module* mod;

	explicit ELF(GRUB_module* mod);

	/**
	 * Get dynamic section onf an ELF
	 * @return dynamic section header table, NULL if an error occurred
	 */
	Elf32_Dyn* get_dyn_table();

	/**
	 * Get the highest address in the runtime address space of an ELF file
	 * @return highest runtime address
	 */
	uint get_highest_runtime_addr();

	/**
	 * Get PLT relocation table of an ELF file
	 * @return PLT relocation table, NULL if error occurred
	 */
	Elf32_Rel* get_rel_plt();

	/**
	 * Get DYNSYM header an ELF file
	 * @return DYNSYM header, NULL if error occurred
	 */
	Elf32_Shdr* get_dynsym_hdr();

	/**
	 * Get a symbol of an ELF file
	 * @return symbol, NULL if error occurred
	 */
	Elf32_Sym* get_symbol(const char* symbol_name);

	/**
	 * Checks whether an ELF file is valid and supported
	 * @param mod GRUB module containing the ELF
	 * @param expected_type expected type of the ELF
	 * @return boolean indicating whether the file is valid or not
	 */
	static bool is_valid(GRUB_module* mod, enum ELF_type expected_type);

	/**
	 * Get the interpreter name of an ELF file
	 * @return name of the interpreter, NULL if the ELF doesn't use one
	 */
	char* get_interpreter_name();

	/**
	 * Get entry point of libdynlk in a process' runtime address space
	 * @param proc_num_pages number of pages the process (without libdynl) spans over
	 * @return libdynlk runtime address, NULL if an error occured
	 */
	void* get_libdynlk_main_runtime_addr(uint proc_num_pages);


	/**
	 * Get segment in which the GOT lays
	 * @param file_got_addr GOT address (in the ELF)
	 * @return GOT segment header, NULL if an error occurred
	 */
	Elf32_Phdr* get_GOT_segment(const uint* file_got_addr);

	[[nodiscard]] size_t base_address();

	[[nodiscard]] size_t num_pages();
};

#endif //INCLUDE_OS_ELF_H
