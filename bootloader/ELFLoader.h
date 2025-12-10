#ifndef ELFLOADER_H
#define ELFLOADER_H

#include "ELF.h"
#include "ELF_defines.h"

class ELFLoader
{
    /**
     * Load part of an ELF segment into the process address space and maps it.
     *
     * @param bytes_ptr pointer to bytes to copy
     * @param n num bytes to copy
     * @param h PT_LOAD segment header
     * @param lib_runtime_load_address runtime load address of the library
     * @param copied_bytes counter of bytes processed in current segment
     */
    void
    copy_elf_subsegment_to_address_space(const void* bytes_ptr, uint n, const Elf32_Phdr* h, uint lib_runtime_load_address,
                                         uint& copied_bytes) const;

    /**
     * Process relocations of an ELF.
     * @param elf elf with relocations to process
     * @param elf_runtime_load_address where the elf is loaded at runtime
     */
    void apply_relocations(ELF* elf, uint elf_runtime_load_address) const;

    /**
     * Converts an address in runtime address space to an address in current address space
     * @param runtime_address address in runtime address space
     * @return corresponding load address
     */
    [[nodiscard]] Elf32_Addr runtime_address_to_load_address(Elf32_Addr runtime_address) const;

    void load_elf_code(const ELF* elf, uint load_address, uint runtime_load_address) const;

public:
    ELF* load_elf(void* buf, ELF_type expected_type);
};


#endif //ELFLOADER_H
