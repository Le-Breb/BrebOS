#include "ELFLoader.h"

#include "../core/fb.h"
#include "../file_management/VFS.h"
#include <kstring.h>
#include "scheduler.h"

ELFLoader::ELFLoader(): current_process(Scheduler::get_running_process()), elf_dep_list(new list<elf_dependence>),
page_tables((Memory::page_table_t*)Memory::calloca(768, sizeof(Memory::page_table_t))),
pdt((Memory::pdt_t*)Memory::malloca(sizeof(Memory::pdt_t)))
{
    init_fini = init_fini_info();
}

ELFLoader::~ELFLoader()
{
    if (used)
        return;

    for (auto i = 0; i < elf_dep_list->size(); i++)
        delete elf_dep_list->get(i)->elf;

    delete elf_dep_list;
    Memory::freea(page_tables);
    Memory::freea(pdt);
}

ELF* ELFLoader::load_libdynlk()
{
    auto libdynlk_file = VFS::browse_to(LIBDYNLK_PATH);
    ELF* libdynlk;
    if (!((libdynlk = load_elf(libdynlk_file, SharedObject))))
        return nullptr;

    return libdynlk;
}

bool ELFLoader::dynamic_loading(const ELF* elf)
{
    if (elf->interpreter_name == nullptr)
        return true;

    irrecoverable_error("Dynamically linked ELFs are not supported (yet), cannot load program");

    // Get libdynlk entry point
    void* libdynlk_entry_point = (void*)get_libdynlk_runtime_address();
    if (libdynlk_entry_point == nullptr)
    {
        printf_error("Error while loading libdynlk");
        return false;
    }

    // Load lib
    auto lib = VFS::browse_to(LIBK_PATH);
    if (!load_elf(lib, SharedObject))
        return false;

    return true;
}

void ELFLoader::apply_relocations(ELF* elf, uint elf_runtime_load_address) const
{
    // Cf. Relocation Types in http://www.skyfree.org/linux/references/ELF_Format.pdf
    uint& B = elf_runtime_load_address;
    uint elf_load_address = runtime_address_to_load_address(elf_runtime_load_address);

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
                Elf32_Sym* symbol = &elf->dynsym[symbol_id];
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
                Elf32_Sym* symbol = &elf->dynsym[symbol_id];
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

bool ELFLoader::alloc_elf_memory(const ELF& elf)
{
    // Num pages lib  code spans over
    uint lib_num_code_pages = elf.num_pages();

    offset_memory_mapping(lib_num_code_pages);

    // Allocate lib code pages
    for (int i = 0; i < elf.global_hdr.e_phnum; ++i)
    {
        Elf32_Phdr* h = &elf.prog_hdrs[i];
        if (h->p_type != PT_LOAD)
            continue;

        uint num_segment_pages = (h->p_memsz + PAGE_SIZE - 1) >> 12;
        for (size_t j = 0; j < num_segment_pages; ++j)
        {
            uint runtime_page_id = (h->p_vaddr >> 12) + j;
            uint pte_id = num_pages + runtime_page_id;
            if (PTE(page_tables, pte_id))
                continue;
            uint pe = Memory::get_free_pe_user(); // Get PTE id
            Memory::allocate_page_user<false>(pe); // Allocate page in kernel address space

            // Map page in current process' address space
            uint page_id = (768 * PT_ENTRIES - PROCESS_N_STACKS_PAGES - lib_num_code_pages + runtime_page_id);
            current_process->update_pte(page_id, PTE(Memory::page_tables, pe), true);
        }
    }
    num_pages += lib_num_code_pages;

    return true;
}

void
ELFLoader::copy_elf_subsegment_to_address_space(const void* bytes_ptr, uint n, const Elf32_Phdr* h,
                                                uint elf_runtime_load_address,
                                                uint& copied_bytes) const
{
    auto runtime_address = elf_runtime_load_address + (h->p_vaddr + copied_bytes);
    auto dest_addr = runtime_address_to_load_address(runtime_address);
    memcpy((void*)dest_addr, bytes_ptr, n);

    // Update counter
    copied_bytes += n;
}

void ELFLoader::register_elf_init_and_fini(const ELF* elf, uint runtime_load_address)
{
    uint init_arr_byte_size = 0;
    uint fini_arr_byte_size = 0;
    Elf32_Addr* init_array = nullptr;
    Elf32_Addr* fini_array = nullptr;
    Elf32_Addr init_runtime_address = 0;
    Elf32_Addr fini_runtime_address = 0;

    // Collect _init, _fini, init_array and fini_array pointer
    for (auto k = 1; k < elf->global_hdr.e_shnum; k++)
    {
        Elf32_Shdr& h = elf->section_hdrs[k];
        const char* section_name = &elf->shstrtab[h.sh_name];
        if (!strcmp(section_name, ".init"))
            init_runtime_address = runtime_load_address + h.sh_addr;
        else if (!strcmp(section_name, ".fini"))
            fini_runtime_address = runtime_load_address + h.sh_addr;
        else if (!strcmp(section_name, ".init_array"))
        {
            Elf32_Addr init_array_runtime_addr = runtime_load_address + h.sh_addr;
            init_arr_byte_size = h.sh_size;
            init_array = (Elf32_Addr*)runtime_address_to_load_address(init_array_runtime_addr);
        }
        else if (!strcmp(section_name, ".fini_array"))
        {
            Elf32_Addr fini_array_runtime_addr = runtime_load_address + h.sh_addr;
            fini_arr_byte_size= h.sh_size;
            fini_array = (Elf32_Addr*)runtime_address_to_load_address(fini_array_runtime_addr);
        }
    }

    // Store init and fini arrays
    uint init_n = init_arr_byte_size / sizeof(Elf32_Addr);
    for (uint i = init_n - 1; i < init_n; i--)
        init_fini.init_array.addFirst(init_array[i]);
    if (init_runtime_address)
        init_fini.init_array.addFirst(init_runtime_address);
    uint fini_n = fini_arr_byte_size / sizeof(Elf32_Addr);
    for (uint i = fini_n - 1; i < init_n; i--)
        init_fini.fini_array.addFirst(fini_array[i]);
    if (fini_runtime_address)
        init_fini.fini_array.addFirst(fini_runtime_address);
}

void ELFLoader::map_elf(const ELF* load_elf, Elf32_Addr runtime_load_address) const
{
    for (int k = 0; k < load_elf->global_hdr.e_phnum; ++k)
    {
        Elf32_Phdr* h = &load_elf->prog_hdrs[k];
        if (h->p_type != PT_LOAD)
            continue;
        uint num_pages = (h->p_memsz + PAGE_SIZE - 1) >> 12;

        // Map code
        for (uint i = 0; i < num_pages; i++)
        {
            Elf32_Addr runtime_address = runtime_load_address + h->p_vaddr + i * PAGE_SIZE;
            uint page_id = runtime_address_to_load_address(runtime_address) >> 12;
            uint pde = ADDR_PDE(runtime_address);
            uint pte = ADDR_PTE(runtime_address);
            page_tables[pde].entries[pte] = PTE(current_process->page_tables, page_id);

            // Check if page should have write permissions, and remove the permission if it's not the case
            if (!(h->p_flags & PF_W))
                page_tables[pde].entries[pte] &= ~PAGE_WRITE;
        }
    }
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

void ELFLoader::setup_elf_got(const ELF* elf, uint elf_runtime_load_address) const
{
    if (libdynlk_runtime_entry_point == ELF32_ADDR_ERR)
        return;

    auto got_runtime_addr = elf_runtime_load_address + elf->runtime_got_addr;

    //printf("%s GOT: 0x%x\n", path, got_runtime_addr);

    // Write GOT entries 1 and 2 (unique ID and libdynlk runtime entry point)
    auto got_load_addr = (Elf32_Addr*)runtime_address_to_load_address(got_runtime_addr);
    *(void**)(got_load_addr + 1) = (void*)elf;
    *(void**)(got_load_addr + 2) = (void*)libdynlk_runtime_entry_point;
}

Elf32_Addr ELFLoader::get_libdynlk_runtime_address()
{
    if (libdynlk_runtime_entry_point != ELF32_ADDR_ERR)
        return libdynlk_runtime_entry_point;

    uint proc_num_pages = num_pages;
    ELF* libdynlk_elf;
    if (!((libdynlk_elf = load_libdynlk())))
        return -1;

    // Find and return libdynlk runtime entry point
    uint libdynlk_load_address = runtime_address_to_load_address(proc_num_pages << 12);
    Elf32_Sym* s = libdynlk_elf->get_dynamic_symbol("lib_main", libdynlk_load_address);
    return libdynlk_runtime_entry_point =
        s == nullptr ? ELF32_ADDR_ERR : proc_num_pages * PAGE_SIZE + s->st_value;
}

void ELFLoader::setup_stacks(Elf32_Addr& k_stack_top, Elf32_Addr& p_stack_top_v_addr) const
{
    // Allocate process stack pages
    for (int i = 0; i < PROCESS_STACK_N_PAGES; i++)
    {
        uint p_stack_pe_id = Memory::get_free_pe_user();
        Memory::allocate_page_user<false>(p_stack_pe_id);

        // Map it in current process' address space below first code page
        uint p_stack_page_id = 768 * PT_ENTRIES - (PROCESS_N_STACKS_PAGES + num_pages) - PROCESS_STACK_N_PAGES + i;
        uint p_stack_pde = p_stack_page_id / PT_ENTRIES;
        uint p_stack_pte = p_stack_page_id % PT_ENTRIES;
        current_process->page_tables[p_stack_pde].entries[p_stack_pte] = PTE(Memory::page_tables, p_stack_pe_id);
        INVALIDATE_PAGE(p_stack_pde, p_stack_pte);

        // Map it in destination process' address space
        uint dest_page_id = 768 * PT_ENTRIES - PROCESS_STACK_N_PAGES + i;
        uint dest_pde = dest_page_id / PT_ENTRIES;
        uint dest_pte = dest_page_id % PT_ENTRIES;
        if (page_tables[dest_pde].entries[dest_pte] != 0)
            irrecoverable_error("%s: Process kernel stack page entry is not empty", __FUNCTION__);
        page_tables[dest_pde].entries[dest_pte] = PTE(Memory::page_tables, p_stack_pe_id);
    }

    // Compute stack top
    p_stack_top_v_addr = (768 * PT_ENTRIES - (PROCESS_N_STACKS_PAGES + num_pages)) << 12;

    // Allocate syscall handler stack pages
    for (int i = 0; i < PROCESS_SYSCALL_STACK_N_PAGES; i++)
    {
        uint k_stack_pe = Memory::get_free_pe_user();
        Memory::allocate_page<false>(k_stack_pe);

        // Map it in destination process' address space
        uint dest_page_id = 768 * PT_ENTRIES - PROCESS_STACK_N_PAGES - PROCESS_SYSCALL_STACK_N_PAGES + i;
        uint dest_pde = dest_page_id / PT_ENTRIES;
        uint dest_pte = dest_page_id % PT_ENTRIES;
        if (page_tables[dest_pde].entries[dest_pte] != 0)
            irrecoverable_error("%s: Process kernel stack page entry is not empty", __FUNCTION__);
        page_tables[dest_pde].entries[dest_pte] = PTE(Memory::page_tables, k_stack_pe);
    }

    k_stack_top = ((768 * PT_ENTRIES - PROCESS_STACK_N_PAGES) << 12) - sizeof(int);

    // Update relevant PDT entries
    for (int i = 0; i < (PROCESS_N_STACKS_PAGES + PT_ENTRIES - 1) / PT_ENTRIES; i++)
        pdt->entries[767 - i] = PHYS_ADDR(Memory::page_tables, (uint) &page_tables[767 - i]) | PAGE_USER | PAGE_WRITE |
                PAGE_PRESENT;
}

void ELFLoader::setup_pcb(uint p_stack_top_v_addr, int argc, const char** argv)
{
    stack_state.eip = elf_dep_list->get(0)->elf->global_hdr.e_entry;
    stack_state.ss = 0x20 | 0x03;
    stack_state.cs = 0x18 | 0x03;
    stack_state.esp = write_args_to_stack(p_stack_top_v_addr, argc, argv);
    stack_state.eflags = 0x200;
    stack_state.error_code = 0;
}

void ELFLoader::unmap_new_process_from_current_process() const
{
    for (size_t i = 0; i < num_pages + PROCESS_STACK_N_PAGES; i++) // unmapping ELF + process stack
    {
        uint page_id = 767 * PT_ENTRIES + PT_ENTRIES - PROCESS_N_STACKS_PAGES - 1 - i;
        uint pde = page_id / PT_ENTRIES;
        uint pte = page_id % PT_ENTRIES;
        current_process->page_tables[pde].entries[pte] = 0;
        INVALIDATE_PAGE(pde, pte);
    }
}

void ELFLoader::setup_pdt()
{
    // Set process PDT entries: entries 0 to 767 map the process address space using its own page tables
    // Entries 768 to 1024 point to kernel page tables, so that kernel is mapped. Moreover, syscall handlers
    // will not need to switch to kernel pdt to make changes in kernel memory as it is mapped the same way
    // in every process' PDT
    auto num_used_paged_tables = (num_pages + PT_ENTRIES - 1)  / PT_ENTRIES;
    for (size_t i = 0; i < num_used_paged_tables; ++i)
        pdt->entries[i] = PHYS_ADDR(Memory::page_tables, (uint) &page_tables[i]) | PAGE_USER | PAGE_WRITE |
            PAGE_PRESENT;
    for (size_t i = num_used_paged_tables; i < 768; i++)
        pdt->entries[i] = 0;
    // Use kernel page tables for the rest
    for (int i = 768; i < PDT_ENTRIES; ++i)
        pdt->entries[i] = PHYS_ADDR(Memory::page_tables, (uint) &Memory::page_tables[i]) | PAGE_WRITE | PAGE_PRESENT;
}

__attribute__((no_instrument_function))
uint64_t rdtsc()
{
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

char* ELFLoader::write_auxv(char* esp, const char* bin_path) const
{
    const ELF& elf = *elf_dep_list->get(0)->elf;

    char** execfn = nullptr;
    char** random = nullptr;

    // The last entry is AT_NULL to indicate the end of the vector
    esp -= sizeof(auxv_t);
    *(auxv_t*)esp = {AT_NULL, (long)0};

    esp -= sizeof(auxv_t);
    *(auxv_t*)esp = {AT_PAGESZ, PAGE_SIZE};

    esp -= sizeof(auxv_t);
    *(auxv_t*)esp = {AT_SECURE, (long)0};

    esp -= sizeof(auxv_t);
    uint phdr_runtime_addr = elf_dep_list->get(0)->runtime_load_address + elf.num_pages() * PAGE_SIZE;
    *(auxv_t*)esp = {AT_PHDR, (long)phdr_runtime_addr};

    esp -= sizeof(auxv_t);
    *(auxv_t*)esp = {AT_PHENT, elf.global_hdr.e_phentsize};

    esp -= sizeof(auxv_t);
    *(auxv_t*)esp = {AT_PHNUM, elf.global_hdr.e_phnum};

    esp -= sizeof(auxv_t);
    execfn = (char**)esp;
    *(auxv_t*)esp = {AT_EXECFN, (void(*)())nullptr};

    esp -= sizeof(auxv_t);
    random = (char**)esp;
    *(auxv_t*)esp = {AT_RANDOM,  (void*)nullptr};

    auto bin_path_len = strlen(bin_path);
    esp -= bin_path_len + 1;
    memcpy(esp, bin_path, bin_path_len + 1);
    *execfn = esp;
    ((auxv_t*)execfn)->a_ptr = esp;

    // Pseudo random bytes generation using rdtsc
    ((auxv_t*)random)->a_ptr = esp - 1;
    for (int i = 0; i < 4; i++)
    {
        auto v = rdtsc();
        for (int j = 0; j < 4; j++)
        {
            *--esp = v & 0xFF;
            v >>= 8;
        }
    }

    return esp;
}

ELF* ELFLoader::load_elf(const SharedPointer<Dentry>& file, ELF_type expected_type)
{
    auto buf = VFS::load_file(file);
    if (!buf)
        return nullptr;

    auto elf = load_elf(buf, expected_type);
    delete[] (char*)buf;

    return elf;
}

ELF* ELFLoader::load_elf(void* buf, ELF_type expected_type)
{
    ELF* elf;
    if (!((elf = ELF::is_valid((uint)buf, expected_type))))
        return nullptr;

    uint load_address = (uint)buf;
    uint runtime_load_addr = num_pages * PAGE_SIZE;
    elf_dep_list->add({elf, runtime_load_addr});

    alloc_elf_memory(*elf);
    map_elf(elf, runtime_load_addr);
    load_elf_code(elf, load_address, runtime_load_addr);
    apply_relocations(elf, runtime_load_addr);
    register_elf_init_and_fini(elf, runtime_load_addr);
    if (!dynamic_loading(elf))
        return nullptr;
    setup_elf_got(elf, runtime_load_addr);

    return elf;
}

Process* ELFLoader::build_process(int argc, const char** argv, pid_t pid, pid_t ppid,
                                  const SharedPointer<Dentry>& file, uint priority)
{
    if (!load_elf(file, Executable))
        return nullptr;

    auto k_stack_top = finalize_process_setup(argc, argv);

    used = true;
    return new Process(file->get_absolute_path(), num_pages, elf_dep_list, page_tables, pdt, &stack_state, priority, pid, ppid, k_stack_top);
}

void ELFLoader::load_phdr()
{
    ELF& elf = *elf_dep_list->get(0)->elf;
    const uint n_phdrs = elf.global_hdr.e_phnum;
    uint phdrs_num_pages = (n_phdrs * sizeof(Elf32_Phdr) + PAGE_SIZE - 1) >> 12;

    offset_memory_mapping(phdrs_num_pages);

    // Allocate memory in current process
    for (uint i = 0; i < phdrs_num_pages; ++i)
    {
        uint pe = Memory::get_free_pe_user(); // Get PTE id
        Memory::allocate_page_user<false>(pe); // Allocate page in kernel address space
        uint page_id = (768 * PT_ENTRIES - PROCESS_N_STACKS_PAGES - phdrs_num_pages + i);
        current_process->update_pte(page_id, PTE(Memory::page_tables, pe), true);
    }
    num_pages += phdrs_num_pages;

    // Map memory in loaded process address space
    uint offset = elf_dep_list->get(0)->runtime_load_address + elf.num_pages() * PAGE_SIZE;
    for (uint i = 0; i < phdrs_num_pages; ++i)
    {
        uint runtime_address = offset + PAGE_SIZE * i;
        uint page_id = ADDR_PAGE(runtime_address_to_load_address(runtime_address));
        uint pde = ADDR_PDE(runtime_address);
        uint pte = ADDR_PTE(runtime_address);
        page_tables[pde].entries[pte] = PTE(current_process->page_tables, page_id);
    }

    Elf32_Phdr dummy_hdr{};
    dummy_hdr.p_vaddr = offset; // Hacky way of providing the offset
    uint _unused = 0; // Must be set to 0 to not mess with address computations
    uint elf_runtime_load_address = elf_dep_list->get(0)->runtime_load_address;
    copy_elf_subsegment_to_address_space(elf.prog_hdrs, n_phdrs * sizeof(Elf32_Phdr), &dummy_hdr, elf_runtime_load_address, _unused);
}

void ELFLoader::write_to_stack(char*& stack_top, const void* content, size_t size)
{
    stack_top -= size;
    memcpy(stack_top, content, size);
}

Elf32_Addr ELFLoader::finalize_process_setup(int argc, const char** argv)
{
    Elf32_Addr k_stack_top, p_stack_top_v_addr;

    setup_pdt();
    load_phdr();
    setup_stacks(k_stack_top, p_stack_top_v_addr);
    setup_pcb(p_stack_top_v_addr, argc, argv);
    unmap_new_process_from_current_process();

    return k_stack_top;
}

Elf32_Addr ELFLoader::runtime_address_to_load_address(Elf32_Addr runtime_address) const
{
    uint page_sub = PROCESS_N_STACKS_PAGES + num_pages - (runtime_address >> 12);
    uint pde = (767 - page_sub / PT_ENTRIES);
    uint pte = PT_ENTRIES - page_sub % PT_ENTRIES;
    uint off = runtime_address % PAGE_SIZE;

    return VIRT_ADDR(pde, pte, off);
}

void ELFLoader::offset_memory_mapping(uint offset) const
{
    // Offset pages
    for (size_t i = 0; i < num_pages; i++)
    {
        uint page_id = 768 * PT_ENTRIES - PROCESS_N_STACKS_PAGES - num_pages + i;
        uint new_page_id = page_id - offset;

        current_process->update_pte(new_page_id, PTE(current_process->page_tables, page_id), true);
    }
    // Zero out 'offset' pages on the right
    for (size_t i = 0; i < offset; i++)
    {
        uint page_id = 768 * PT_ENTRIES - PROCESS_N_STACKS_PAGES - offset + i;
        current_process->update_pte(page_id, 0, true);
    }
}

Process* ELFLoader::setup_elf_process(pid_t pid, pid_t ppid, int argc, const char** argv,
                                      const SharedPointer<Dentry>& file, uint priority)
{
    ELFLoader loader{};
    Process* proc = loader.build_process(argc, argv, pid, ppid, file, priority);

    return proc;
}

size_t ELFLoader::write_args_to_stack(size_t stack_top_v_addr, int argc, const char** argv) const
{
    // CF https://uclibc.org/docs/psABI-i386.pdf section 2.11
    char* stack_top = (char*)stack_top_v_addr;
    constexpr char nulls[4]{};

    if (argc != 0 && argv[argc - 1] == nullptr)
        argc--;

    // Write argv contents and argv array
    for (int i = argc - 1; i >= 0; i--)
        write_to_stack(stack_top, argv[i], strlen(argv[i]) + 1);

    // auxv
    stack_top = write_auxv(stack_top, argv[0]);

    // 4 zeros
    write_to_stack(stack_top, nulls, sizeof(nulls));

    // Environment pointers

    // 4 zeroes
    write_to_stack(stack_top, nulls, sizeof(nulls));

    // Argv
    write_to_stack(stack_top, nulls, sizeof(nulls)); // Argv trailing zeroes
    char* argv_ptr = (char*)KERNEL_VIRTUAL_BASE;
    for (int i = argc - 1; i >= 0; i--)
    {
        argv_ptr -= strlen(argv[i]) + 1;
        write_to_stack(stack_top, &argv_ptr, sizeof(argv_ptr));
    }

    // argc
    write_to_stack(stack_top, &argc, sizeof(argc));

    return KERNEL_VIRTUAL_BASE - (stack_top_v_addr - (uint)stack_top);
}
