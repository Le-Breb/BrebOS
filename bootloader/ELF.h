#ifndef INCLUDE_ELF_H
#define INCLUDE_ELF_H

#include "ELF_defines.h"
#include "config.h"

#define OS_INTERPR ("/dynlk") // OS default interpreter, used to run dynamically linked programs
#define OS_LIB ("libk.so") // OS lib for syscalls not found in traditional libc (aka newlib)

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

	[[nodiscard]]
	static unsigned long hash(const unsigned char *name);

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
	const char* shstrtab;
	size_t num_plt_relocs;
	size_t num_dyn_relocs;
	char* lib_name = nullptr;
    Elf32_Addr runtime_got_addr = (Elf32_Addr)-1;
    Elf32_Addr hash_table_runtime_address = (Elf32_Addr)-1;

	explicit ELF(uint start_address);

	~ELF();

	/**
	 * Get the highest address in the runtime address space of an ELF file
	 * @return highest runtime address
	 */
	[[nodiscard]] uint get_highest_runtime_addr() const;

	/**
	 * Get a symbol of an ELF file
	 * @param symbol_name name of the symbol we look for
	 * @param load_address where is the ELF loaded
	 * @return symbol, NULL if error occurred
	 */
	Elf32_Sym* get_symbol(const char* symbol_name, Elf32_Addr load_address) const;

	/**
	 * Checks whether an ELF file is valid and supported
	 * @param start_address GRUB module containing the ELF
	 * @param expected_type expected type of the ELF
	 * @return ELF* if it is valid, nullptr otherwise
	 */
	static ELF* is_valid(uint start_address, ELF_type expected_type);

	[[nodiscard]] size_t base_address() const;

	[[nodiscard]] size_t num_pages() const;
};

#endif //INCLUDE_ELF_H