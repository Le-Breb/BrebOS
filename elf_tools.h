#ifndef INCLUDE_ELF_TOOLS_H
#define INCLUDE_ELF_TOOLS_H

#include "process.h"

/**
 * Create a process to run an ELF executable
 *
 * @param module Grub module id
 * @param grub_modules grub modules
 * @return program's process
 */
process* load_elf(unsigned int module, GRUB_module* grub_modules);

/**
 * Get dynamic section onf an ELF
 * @param mod GRUB module containing the ELF
 * @param elf32Ehdr ELF header
 * @param elf32Shdr section header table
 * @return dynamic section header table, NULL if an error occurred
 */
Elf32_Dyn* get_elf_dyn_table(elf_info* elfi);

/**
 * Get the highest address in the runtime address space of an ELF file
 * @param elf32Ehdr ELF header
 * @param elf32Phdr program header table
 * @return highest runtime address
 */
unsigned int get_elf_highest_runtime_addr(elf_info* elfi);

/**
 * Get PLT relocation table of an ELF file
 * @param elfi ELF info
 * @return PLT relocation table, NULL if error occured
 */
Elf32_Rel* get_elf_rel_plt(elf_info* elfi);

/**
 * Get DYNSYM header an ELF file
 * @param elfi ELF info
 * @return DYNSYM header, NULL if error occured
 */
Elf32_Shdr* get_elf_dynsym_hdr(elf_info* elfi);

/**
 * Get a symbol of an ELF file
 * @param elfi ELF info
 * @return symbol, NULL if error occured
 */
Elf32_Sym* get_elf_symbol(elf_info* elfi, char* symbol_name);

#endif //INCLUDE_ELF_TOOLS_H
