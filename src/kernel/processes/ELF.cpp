#include "ELF.h"
#include <kstring.h>
#include "../core/fb.h"
#include "../file_management/VFS.h"

using namespace ELFTools;

#define is_valid_exit_err {delete elf; return nullptr;}

ELF* ELF::is_valid(uint start_address, ELF_type expected_type)
{
    ELF* elf = new ELF(start_address);

    // Ensure this is an ELF
    if (elf->global_hdr.e_ident[0] != 0x7F || elf->global_hdr.e_ident[1] != 'E' || elf->global_hdr.e_ident[2] != 'L' ||
        elf->global_hdr.e_ident[3] != 'F')
    {
        printf_error("Not an ELF");
        is_valid_exit_err
    }

    // Ensure program is 32 bits
    if (elf->global_hdr.e_ident[4] != 1)
    {
        printf_error("Not a 32 bit program");
        is_valid_exit_err
    }

    switch (expected_type)
    {
    case Executable:
        if (elf->global_hdr.e_type != ET_EXEC)
        {
            printf_error("Not an executable ELF");
            is_valid_exit_err
        }
        break;
    case SharedObject:
        if (elf->global_hdr.e_type != ET_DYN)
        {
            printf_error("Not an executable ELF");
            is_valid_exit_err
        }
        break;
    case Relocatable:
        if (elf->global_hdr.e_type != ET_REL)
        {
            printf_error("Not an executable ELF");
            is_valid_exit_err
        }
        break;
    }
    //printf("Endianness: %s\n", elf32Ehdr->e_ident[5] == 1 ? "Little" : "Big");

    // ELF imposes that first section header is null
    if (elf->section_hdrs[0].sh_type != SHT_NULL)
    {
        printf_error("Not a valid ELF file");
        is_valid_exit_err
    }

    // Ensure program is supported
    bool versioning_ignored = false;
    for (int k = 0; k < elf->global_hdr.e_shnum; ++k)
    {
        Elf32_Shdr* h = &elf->section_hdrs[k];
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
        case SHT_INIT_ARRAY:
        case SHT_FINI_ARRAY:
            break;
        case SHT_GNU_versym: case SHT_GNU_verneed: case SHT_GNU_verdef:
        {
            // Those sections are very likely to be found in ELFs dynamically linked against libstdc++-v3
            // The latter been compiled by using Linux configuration (cf sed in gcc_setup in toolchain_setup.sh).
            // I guess Linux does use versioning because of glibc, resulting in those sections here.
            // It is then reasonable to suppose that ignoring that information is harmless
            versioning_ignored = true;
            break;
        }
        default:
            printf_error("Section type not supported: %i. Aborting", h->sh_type);
            is_valid_exit_err
        }
    }
    if (versioning_ignored)
        printf_warn("ELF versionning info ignored. Likely harmless, but we never know...");

    if (elf->global_hdr.e_phnum == 0)
    {
        printf_error("No program header");
        is_valid_exit_err
    }

    // Check that program headers types are supported
    constexpr Elf32_Word supported_phdr_types[] = {PT_LOAD, PT_INTERP, PT_TLS, PT_GNU_EH_FRAME, PT_GNU_STACK, PT_DYNAMIC, PT_PHDR};
    for (int k = 0; k < elf->global_hdr.e_phnum; ++k)
    {
        Elf32_Phdr* h = &elf->prog_hdrs[k];

        bool supported = false;
        for (const auto supported_type : supported_phdr_types)
            if (supported_type == h->p_type)
                {supported = true; break;}
        if (!supported)
            is_valid_exit_err
    }

    // Check that no read-only page overlaps with a write-allowed page
    uint elf_num_pages = elf->num_pages();
    bool* ro_pages = (bool*)calloc(elf_num_pages, sizeof(bool));
    for (int k = 0; k < elf->global_hdr.e_phnum; ++k)
    {
        Elf32_Phdr* h = &elf->prog_hdrs[k];
        if (h->p_type != PT_LOAD)
            continue;

        bool segment_write = h->p_flags & PF_W;

        uint n_pages = (h->p_memsz + PAGE_SIZE - 1) >> 12;
        for (size_t j = 0; j < n_pages; ++j)
        {
            uint page_id = (h->p_vaddr >> 12) + j;
            if (segment_write)
            {
                if (ro_pages[page_id])
                {
                    free(ro_pages);
                    is_valid_exit_err
                }
            }
            else
                ro_pages[page_id] = true;
        }
    }
    free(ro_pages);

    size_t base_addr = elf->base_address();
    /*if (base_addr < PAGE_SIZE)
    {
        printf_error("Base address (%zu) < PAGE_SIZE (%u). Aborting", base_addr, PAGE_SIZE);
        is_valid_exit_err
    }*/
    if (base_addr == (size_t)-1)
    {
        printf_error("Cannot compute base address. Aborting");
        is_valid_exit_err
    }

    if (!elf->interpreter_name)
        return elf;

    if (elf->dyn_table == nullptr)
    {
        printf_error("No dynamic symbol table");
        is_valid_exit_err
    }

    if (!VFS::browse_to(elf->interpreter_name, true, false))
    {
        printf_error("Missing interpreter: %s", elf->interpreter_name);
        is_valid_exit_err
    }

    if (elf->runtime_got_addr == ELF32_ADDR_ERR)
    {
        printf_error("No GOT");
        is_valid_exit_err
    }

    return elf;
}

uint ELF::get_highest_runtime_addr() const
{
    uint highest_addr = 0;
    for (int k = 0; k < global_hdr.e_phnum; ++k)
    {
        Elf32_Phdr* h = &prog_hdrs[k];
        if (h->p_type != PT_LOAD)
            continue;

        uint high_addr = h->p_vaddr + h->p_memsz;
        if (high_addr > highest_addr)
            highest_addr = high_addr;
    }

    return highest_addr;
}

Elf32_Sym* ELF::get_dynamic_symbol(const char* symbol_name, Elf32_Addr load_address, const list<alloc>& allocations) const
{
    // If there is no hash table available, linearly search for the symbol
    if (hash_table_runtime_address == ELF32_ADDR_ERR)
    {
        uint lib_dynsym_num_entries = dynsym_hdr->sh_size / dynsym_hdr->sh_entsize;
        for (uint i = 1; i < lib_dynsym_num_entries; i++)
        {
            if (strcmp(&dynsym_strtab[dynsym[i].st_name], symbol_name) == 0)
                return &dynsym[i];
        }
    }
    else // Otherwise, make fast lookup during the hash table
    {
        const auto h = hash((const unsigned char*)symbol_name);
        Lptr<uint> hash_table = Lptr((uint*)(load_address + hash_table_runtime_address), &allocations);
        uint nbucket = hash_table[0];
        Lptr<uint> buckets = hash_table + 2U;
        Lptr<uint> chain = buckets + nbucket;
        uint y = buckets[h % nbucket];

        while (y != STN_UNDEF && strcmp(&dynsym_strtab[dynsym[y].st_name], symbol_name) != 0)
            y = chain[y];

        return y == STN_UNDEF ? nullptr : &dynsym[y];
    }

    return nullptr;
}

Elf32_Sym* ELF::get_symbol(const char* symbol_name) const
{
    uint symtab_num_entries = symtab_hdr->sh_size / symtab_hdr->sh_entsize;
    for (uint i = 1; i < symtab_num_entries; i++)
    {
        if (strcmp(&strtab[symtab[i].st_name], symbol_name) == 0)
            return &symtab[i];
    }

    return nullptr;
}

ELF::ELF(Elf32_Ehdr* elf32Ehdr, Elf32_Phdr* elf32Phdr, Elf32_Shdr* elf32Shdr,
         uint start_address) : global_hdr(*elf32Ehdr)
{
    prog_hdrs = elf32Phdr;
    section_hdrs = elf32Shdr;

    for (int k = 0; k < this->global_hdr.e_shnum; ++k)
    {
        switch (Elf32_Shdr* h = &section_hdrs[k]; h->sh_type)
        {
            case SHT_DYNAMIC:
                dyn_table = (Elf32_Dyn*)(start_address + h->sh_offset);
                break;
            case SHT_SYMTAB:
            {
                symtab_hdr = h;
                symtab = (Elf32_Sym*)(start_address + symtab_hdr->sh_offset);

                Elf32_Shdr* strtab_hdr = (Elf32_Shdr*)((uint)elf32Shdr + elf32Ehdr->e_shentsize * symtab_hdr->sh_link);
                strtab = (char*)start_address + strtab_hdr->sh_offset;
                break;
            }
            case SHT_DYNSYM:
            {
                dynsym_hdr = h;
                Elf32_Shdr* dyn_strtab_h = &section_hdrs[h->sh_link];
                dynsym_strtab = (char*)start_address + dyn_strtab_h->sh_offset;
                break;
            }
            default:
                break;
        }
    }


    for (int k = 0; k < this->global_hdr.e_phnum; ++k)
    {
        const Elf32_Phdr* h = &prog_hdrs[k];

        if (h->p_type != PT_INTERP)
            continue;

        interpreter_name = (char*)start_address + h->p_offset;
        break;
    }

    Elf32_Shdr* sh_strtab_h = (Elf32_Shdr*)((uint)elf32Shdr +
        elf32Ehdr->e_shentsize * elf32Ehdr->e_shstrndx);
    shstrtab = (char*)start_address + sh_strtab_h->sh_offset;

    dynsym = dynsym_hdr ? (Elf32_Sym*)(start_address + dynsym_hdr->sh_offset) : nullptr;

    // In order to know whether base address should be retrieved from values, refer to Figure 2-10: Dynamic Array Tags
    // in http://www.skyfree.org/linux/references/ELF_Format.pdf
    if (dyn_table)
    {
        for (const Elf32_Dyn* d = dyn_table; d->d_tag != DT_NULL; d++)
        {
            switch (d->d_tag)
            {
                case DT_PLTGOT:
                    runtime_got_addr = d->d_un.d_val;
                    break;
                case DT_HASH:
                {
                    hash_table_runtime_address = d->d_un.d_val;
                    break;
                }
                default:
                    break;
            }
        }
    }
}

ELF::ELF(uint start_address)
    : ELF((Elf32_Ehdr*)start_address,
          (Elf32_Phdr*)(start_address + ((Elf32_Ehdr*)start_address)->e_phoff),
          (Elf32_Shdr*)(start_address + ((Elf32_Ehdr*)start_address)->e_shoff),
          start_address)
{
}

ELF::~ELF()
{
}

size_t ELF::base_address() const
{
    size_t lowest_address = -1;
    for (int k = 0; k < global_hdr.e_phnum; ++k)
    {
        const Elf32_Phdr* h = &prog_hdrs[k];
        if (h->p_type != PT_LOAD)
            continue;

        if (const uint addr = h->p_vaddr; addr < lowest_address)
            lowest_address = addr;
    }

    return lowest_address & ~(PAGE_SIZE - 1); // Truncate to nearest page size multiple
}

size_t ELF::num_pages() const
{
    return (get_highest_runtime_addr() + PAGE_SIZE - 1) >> 12;
}
