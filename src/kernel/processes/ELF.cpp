#include "ELF.h"
#include <kstring.h>
#include "../core/fb.h"

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
        default:
            printf_error("Section type not supported: %i. Aborting", h->sh_type);
            is_valid_exit_err
        }
    }

    for (size_t i = 0; i < elf->num_plt_relocs; i++)
        if (auto reloc_type = ELF32_R_TYPE(elf->plt_relocs[i].r_info); reloc_type != R_386_JMP_SLOT)
        {
            printf_error("Unsupported PLT relocation type: %u. Aborting", reloc_type);
            is_valid_exit_err
        }
    for (size_t i = 0; i < elf->num_dyn_relocs; i++)
        if (auto reloc_type = ELF32_R_TYPE(elf->dyn_relocs[i].r_info); reloc_type != R_386_RELATIVE && reloc_type !=
            R_386_GLOB_DAT && reloc_type != R_386_32)
        {
            if (reloc_type == R_386_PC32)
            {
                uint symbol_id = ELF32_R_SYM(elf->dyn_relocs[i].r_info);
                Elf32_Sym* symbol = &elf->symbols[symbol_id];

                // Weak symbols that are not yet defined are not an issue
                if (ELF32_ST_BIND(symbol->st_info) != STB_WEAK || symbol->st_shndx != 0)
                {
                    printf_error("unsupported R_396_PC32 relocation");
                    is_valid_exit_err
                }
                continue;
            }
            printf_error("Unsupported dyn relocation type: %u. Aborting", reloc_type);
            is_valid_exit_err
        }

    if (elf->global_hdr.e_phnum == 0)
    {
        printf_error("No program header");
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

    if (strcmp(elf->interpreter_name, OS_INTERPR) != 0)
    {
        printf_error("Unsupported interpreter: %s", elf->interpreter_name);
        is_valid_exit_err
    }

    if (elf->runtime_got_addr == ELF32_ADDR_ERR)
    {
        printf_error("No GOT");
        is_valid_exit_err
    }

    // Get needed lib name and check it is not missing
    char* lib_name = elf->lib_name;
    if (strcmp(lib_name, OS_LIB) != 0)
    {
        printf_error("Missing library: %s", lib_name);
        is_valid_exit_err
    }

    // Ensure dynamic symbols are supported
    uint dynsym_num_entries = elf->dynsym_hdr->sh_size / elf->dynsym_hdr->sh_entsize;
    for (uint i = 1; i < dynsym_num_entries; ++i) // First entry has to be null, we skip it
    {
        Elf32_Sym* dd = elf->symbols + i;
        //printf("%s\n", &dyn_str_table[dd->st_name]);
        uint type = ELF32_ST_TYPE(dd->st_info);
        if (type != STT_FUNC && type != STT_NOTYPE) // _init and _fini have type STT_NOTYPE
        {
            printf_error("Unsupported symbol type: %u", type);
            is_valid_exit_err
        }
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

Elf32_Sym* ELF::get_symbol(const char* symbol_name, Elf32_Addr load_address) const
{
    // If there is not hash table available, linearly search for the symbol
    if (hash_table_runtime_address == ELF32_ADDR_ERR)
    {
        uint lib_dynsym_num_entries = dynsym_hdr->sh_size / dynsym_hdr->sh_entsize;
        for (uint i = 1; i < lib_dynsym_num_entries; i++)
        {
            if (strcmp(&dynsym_strtab[symbols[i].st_name], symbol_name) == 0)
                return &symbols[i];
        }
    }
    else // Otherwise, make fast lookup during the hash table
    {
        auto h = hash((const unsigned char*)symbol_name);
        uint* hash_table = (uint*)(load_address + hash_table_runtime_address);
        uint nbucket = *hash_table;
        uint* buckets = hash_table + 2;
        uint* chain = buckets + nbucket;
        uint y = buckets[h % nbucket];

        while (y != STN_UNDEF && strcmp(&dynsym_strtab[symbols[y].st_name], symbol_name) != 0)
            y = chain[y];

        return y == STN_UNDEF ? nullptr : &symbols[y];
    }

    return nullptr;
}

ELF::ELF(Elf32_Ehdr* elf32Ehdr, Elf32_Phdr* elf32Phdr, Elf32_Shdr* elf32Shdr,
         uint start_address) : global_hdr(*elf32Ehdr)
{
    // Copy phdrs
    prog_hdrs = new Elf32_Phdr[elf32Ehdr->e_phnum];
    for (int k = 0; k < this->global_hdr.e_phnum; ++k)
        prog_hdrs[k] = *(Elf32_Phdr*)((uint)elf32Phdr + this->global_hdr.e_phentsize * k);

    // Copy shdrs
    section_hdrs = new Elf32_Shdr[elf32Ehdr->e_shnum];
    for (int k = 0; k < this->global_hdr.e_shnum; ++k)
        section_hdrs[k] = *(Elf32_Shdr*)((uint)elf32Shdr + this->global_hdr.e_shentsize * k);

    // Copy dynamic table
    dyn_table = nullptr;
    for (int k = 0; k < this->global_hdr.e_shnum; ++k)
    {
        Elf32_Shdr& h = section_hdrs[k];

        if (h.sh_type != SHT_DYNAMIC)
            continue;
        uint num_entries = h.sh_size / h.sh_entsize;
        dyn_table = new Elf32_Dyn[num_entries];
        Elf32_Dyn* dyn_table = (Elf32_Dyn*)(start_address + h.sh_offset);
        int i = 0;
        for (; dyn_table->d_tag != DT_NULL; dyn_table++, i++)
            this->dyn_table[i] = *dyn_table;
        this->dyn_table[i] = *dyn_table;

        break;
    }

    // Copy interpreter path
    interpreter_name = nullptr;
    for (int k = 0; k < this->global_hdr.e_phnum; ++k)
    {
        Elf32_Phdr* h = &prog_hdrs[k];

        if (h->p_type != PT_INTERP)
            continue;

        char* interpr_path = (char*)start_address + h->p_offset;
        interpreter_name = new char[strlen(interpr_path) + 1];
        strcpy((char*)interpreter_name, interpr_path);
        break;
    }

    // Copy strtab
    Elf32_Shdr* sh_strtab_h = (Elf32_Shdr*)((uint)elf32Shdr +
        elf32Ehdr->e_shentsize * elf32Ehdr->e_shstrndx);
    shstrtab = new char[sh_strtab_h->sh_size];
    memcpy((char*)shstrtab, (char*)start_address + sh_strtab_h->sh_offset, sh_strtab_h->sh_size);

    // Copy plt relocation entries
    plt_relocs = nullptr;
    num_plt_relocs = 0;
    for (int k = 0; k < elf32Ehdr->e_shnum; ++k)
    {
        Elf32_Shdr* h = (Elf32_Shdr*)((uint)elf32Shdr + elf32Ehdr->e_shentsize * k);
        if (h->sh_type != SHT_REL || strcmp(".rel.plt", &shstrtab[h->sh_name]) != 0)
            continue;
        Elf32_Rel* r = (Elf32_Rel*)(start_address + h->sh_offset);
        uint num_entries = h->sh_size / h->sh_entsize;
        num_plt_relocs = num_entries;
        plt_relocs = new Elf32_Rel[num_entries];
        for (uint i = 0; i < num_entries; i++)
            plt_relocs[i] = *r++;
        break;
    }

    // Copy dyn relocation entries
    dyn_relocs = nullptr;
    num_dyn_relocs = 0;
    for (int k = 0; k < elf32Ehdr->e_shnum; ++k)
    {
        Elf32_Shdr* h = (Elf32_Shdr*)((uint)elf32Shdr + elf32Ehdr->e_shentsize * k);
        if (h->sh_type != SHT_REL || strcmp(".rel.dyn", &shstrtab[h->sh_name]) != 0)
            continue;
        Elf32_Rel* r = (Elf32_Rel*)(start_address + h->sh_offset);
        uint num_entries = h->sh_size / h->sh_entsize;
        num_dyn_relocs = num_entries;
        dyn_relocs = new Elf32_Rel[num_entries];
        for (uint i = 0; i < num_entries; i++)
            dyn_relocs[i] = *r++;
        break;
    }

    // Copy dynsym header and dynsym strtab
    dynsym_hdr = nullptr;
    dynsym_strtab = nullptr;
    for (int k = 0; k < elf32Ehdr->e_shnum; ++k)
    {
        Elf32_Shdr* h = (Elf32_Shdr*)((uint)elf32Shdr + elf32Ehdr->e_shentsize * k);

        if (h->sh_type != SHT_DYNSYM)
            continue;
        dynsym_hdr = new Elf32_Shdr(*h);
        Elf32_Shdr* dyn_strtab_h = &section_hdrs[h->sh_link];
        dynsym_strtab = new char[dyn_strtab_h->sh_size];
        memcpy((char*)dynsym_strtab, (char*)start_address + dyn_strtab_h->sh_offset, dyn_strtab_h->sh_size);
        break;
    }

    // Copy symbols
    symbols = nullptr;
    if (dynsym_hdr)
    {
        uint lib_dynsym_num_entries = dynsym_hdr->sh_size / dynsym_hdr->sh_entsize;
        symbols = new Elf32_Sym[lib_dynsym_num_entries];
        for (uint i = 0; i < lib_dynsym_num_entries; i++)
        {
            Elf32_Sym* lib_symbol_entry = (Elf32_Sym*)(start_address + dynsym_hdr->sh_offset +
                i * dynsym_hdr->sh_entsize);
            symbols[i] = *lib_symbol_entry;
        }
    }

    // In order to know whether base address should be retrieved from values, refer to Figure 2-10: Dynamic Array Tags
    // in http://www.skyfree.org/linux/references/ELF_Format.pdf
    uint lib_name_idx = (uint)-1; // lib name string table index
    uint base_addr = base_address();
    char* dyn_str_table = nullptr; // string table
    if (dyn_table)
    {
        for (Elf32_Dyn* d = dyn_table; d->d_tag != DT_NULL; d++)
        {
            switch (d->d_tag)
            {
                case DT_STRTAB:
                    dyn_str_table = (char*)(start_address + d->d_un.d_ptr - base_addr);
                    break;
                case DT_NEEDED:
                    lib_name_idx = d->d_un.d_val;
                    break;
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

    if (lib_name_idx != (uint)-1)
    {
        lib_name = &dyn_str_table[lib_name_idx];
        auto cpy = new char[strlen(lib_name) + 1];
        strcpy(cpy, lib_name);
        lib_name = cpy;
    }
}

unsigned long ELF::hash(const unsigned char* name)
{
    unsigned long h = 0, g;
    while (*name)
    {
        h = (h << 4) + *name++;
        if ((g = (h & 0xf0000000)))
            h ^= g >> 24 ;
        h &= ~g ;
    }

    return h ;
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
    delete[] prog_hdrs;
    delete[] section_hdrs;
    delete[] dyn_table;
    delete[] interpreter_name;
    delete[] plt_relocs;
    delete[] dyn_relocs;
    delete dynsym_hdr;
    delete[] symbols;
    delete[] lib_name;
}

size_t ELF::base_address() const
{
    size_t lowest_address = -1;
    for (int k = 0; k < global_hdr.e_phnum; ++k)
    {
        Elf32_Phdr* h = &prog_hdrs[k];
        if (h->p_type != PT_LOAD)
            continue;

        uint addr = h->p_vaddr;
        if (addr < lowest_address)
            lowest_address = addr;
    }

    return lowest_address & ~(PAGE_SIZE - 1); // Truncate to nearest page size multiple
}

size_t ELF::num_pages() const
{
    return (get_highest_runtime_addr() + PAGE_SIZE - 1) >> 12;
}
