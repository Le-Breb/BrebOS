#include "ELFLoader.h"
#include "libc.h"

void ELFLoader::apply_relocations(ELF* elf, uint elf_runtime_load_address) const
{
    // Cf. Relocation Types in http://www.skyfree.org/linux/references/ELF_Format.pdf
    uint& B = elf_runtime_load_address;
    uint elf_load_address = 0x100000;

    // PLT relocations - The OS performs lazy binding, so the relocs are not fully resolved: the OS simply
    // adds the elf runtime load address to GOT entries, so that PLT stubs' first JMP remain valid, calling
    // stub 0 afterwards, which will then call dynlk thanks to the address in GOT[2]. Dynlk will then perform
    // the complete relocation at runtime.
    for (size_t i = 0; i < elf->num_plt_relocs; i++)
    {
        // Get GOT entry runtime address
        Elf32_Rel* reloc = elf->plt_relocs + i;
        if (ELF32_R_TYPE(reloc->r_info) != R_386_JMP_SLOT)
            continue;

        // Find where it has been loaded in memory
        uint got_entry_curr_addr = elf_load_address + reloc->r_offset;

        // Update its value
        *(Elf32_Addr*)got_entry_curr_addr += elf_runtime_load_address;
    }

    // Dynamic relocations
    for (size_t i = 0; i < elf->num_dyn_relocs; i++)
    {
        // Get reloc runtime address
        Elf32_Rel* reloc = elf->dyn_relocs + i;

        auto reloc_type = ELF32_R_TYPE(reloc->r_info);

        // Find where it has been loaded in memory
        Elf32_Addr load_address = elf_load_address + reloc->r_offset;

        switch (reloc_type)
        {
            case R_386_GLOB_DAT:
            {
                uint symbol_id = ELF32_R_SYM(reloc->r_info);
                Elf32_Sym* symbol = &elf->symbols[symbol_id];
                Elf32_Addr symbol_address = symbol->st_value;
                Elf32_Addr S = B + symbol_address;

                // Update its value
                *(Elf32_Addr*)load_address = S;
                break;
            }
            case R_386_RELATIVE:
            {
                Elf32_Addr A = *(Elf32_Addr*)load_address;
                *(Elf32_Addr*)load_address = B + A;
                break;
            }
            case R_386_PC32:
            {
                // Nothing to be done here, ELF::is_valid filters ELFs with unsupported R_386_PC32 relocations

                /*Elf32_Addr symbol_address = symbol->st_value;
                Elf32_Addr S = B + symbol_address;
                [[maybe_unused]] const char* symbol_name = &elf->dynsym_strtab[symbol->st_name];
                Elf32_Addr A = *(Elf32_Addr*)load_address;
                Elf32_Addr P = reloc->r_offset;

                // Update its value
                *(Elf32_Addr*)load_address = S + A - P;*/
                break;
            }
            case R_386_32:
            {
                uint symbol_id = ELF32_R_SYM(reloc->r_info);
                Elf32_Sym* symbol = &elf->symbols[symbol_id];
                // Weak symbols that are not yet defined are not an issue
                if (ELF32_ST_BIND(symbol->st_info) == STB_WEAK && symbol->st_shndx == 0)
                    break;
                Elf32_Addr symbol_address = symbol->st_value;
                //[[maybe_unused]] const char* symbol_name = &elf->dynsym_strtab[symbol->st_name];
                Elf32_Addr S = B + symbol_address;
                Elf32_Addr A = *(Elf32_Addr*)load_address;

                // Update its value
                *(Elf32_Addr*)load_address = S + A;
                break;
            }
            default: // Shouldn't happen, supported relocation types are checked in ELF::is_valid
                break;
        }
    }
}

void
ELFLoader::copy_elf_subsegment_to_address_space(const void* bytes_ptr, uint n, const Elf32_Phdr* h,
                                                uint lib_runtime_load_address,
                                                uint& copied_bytes) const
{
    auto runtime_address = lib_runtime_load_address + (h->p_vaddr + copied_bytes);
    auto dest_addr = runtime_address_to_load_address(runtime_address);
    memcpy((void*)dest_addr, bytes_ptr, n);

    // Update counter
    copied_bytes += n;
}

void ELFLoader::load_elf_code(const ELF* elf, uint load_address, uint runtime_load_address) const
{
    // Copy lib code in allocated space
    for (int k = 0; k < elf->global_hdr.e_phnum; ++k)
    {
        Elf32_Phdr* h = &elf->prog_hdrs[k];
        if (h->p_type != PT_LOAD)
            continue;

        uint copied_bytes = 0;
        void* bytes_ptr = (void*)(load_address + h->p_offset);

        // Copy first bytes if they are not page aligned
        if (h->p_vaddr % PAGE_SIZE)
        {
            uint rem = PAGE_SIZE - h->p_vaddr % PAGE_SIZE;
            uint num_first_bytes = rem > h->p_filesz ? h->p_filesz : rem;
            copy_elf_subsegment_to_address_space(bytes_ptr, num_first_bytes, h, runtime_load_address, copied_bytes);
        }

        // Copy whole pages
        while (copied_bytes + PAGE_SIZE < h->p_filesz)
        {
            bytes_ptr = (void*)(load_address + h->p_offset + copied_bytes);
            copy_elf_subsegment_to_address_space(bytes_ptr, PAGE_SIZE, h, runtime_load_address, copied_bytes);
        }

        // Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
        uint num_final_bytes = h->p_filesz - copied_bytes;
        if (num_final_bytes)
        {
            bytes_ptr = (void*)(load_address + h->p_offset + copied_bytes);
            copy_elf_subsegment_to_address_space(bytes_ptr, num_final_bytes, h, runtime_load_address, copied_bytes);
        }

        // Fill the rest with 0. Very important, this is where bss is.
        if (h->p_memsz > h->p_filesz)
        {
            auto runtime_address = runtime_load_address + (h->p_vaddr + h->p_filesz);
            auto dest_addr = runtime_address_to_load_address(runtime_address);
            memset((void*)dest_addr, 0, h->p_memsz - h->p_filesz);
        }
    }
}

ELF* ELFLoader::load_elf(void* buf, ELF_type expected_type)
{
    ELF* elf;
    if (!((elf = ELF::is_valid((uint)buf, expected_type))))
        return nullptr;

    uint load_address = (uint)buf;
    uint runtime_load_addr = 0;


    load_elf_code(elf, load_address, runtime_load_addr);
    apply_relocations(elf, runtime_load_addr);

    return elf;
}

Elf32_Addr ELFLoader::runtime_address_to_load_address(Elf32_Addr runtime_address) const
{
    return runtime_address - KERNEL_VIRTUAL_BASE;
}
