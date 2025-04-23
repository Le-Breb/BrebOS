#include "ELFLoader.h"

#include "../core/fb.h"
#include "../file_management/VFS.h"
#include <kstring.h>
#include "scheduler.h"

ELFLoader::ELFLoader(): current_process(Scheduler::get_running_process()), elf_dep_list(new list<::elf_dependence_list>),
page_tables((Memory::page_table_t*)Memory::calloca(768, sizeof(Memory::page_table_t))),
pdt((Memory::pdt_t*)Memory::malloca(sizeof(Memory::pdt_t)))
{
    init_fini = init_fini_info();
}

ELFLoader::~ELFLoader()
{
    if (used)
        return;

    delete elf_dep_list;
    Memory::freea(page_tables);
    Memory::freea(pdt);
}

bool ELFLoader::dynamic_loading(ELF* elf)
{
    const char* interpereter_name = elf->interpreter_name;
    if (strcmp(interpereter_name, OS_INTERPR) != 0)
    {
        printf_error("Unsupported interpreter: %s", interpereter_name);
        return false;
    }

    Elf32_Dyn* dyn_table = elf->dyn_table;
    if (dyn_table == nullptr)
        return false;
    size_t base_addr = elf->base_address();
    if (base_addr < PAGE_SIZE)
    {
        printf_error("Process base address (%zu) < PAGE_SIZE (%u). Aborting", base_addr, PAGE_SIZE);
        return false;
    }
    if (base_addr == (size_t)-1)
    {
        printf_error("Cannot compute base address. Aborting");
        return false;
    }

    if (elf->runtime_got_addr == (Elf32_Addr)-1)
    {
        printf_error("No GOT");
        return false;
    }

    // Get needed lib name and check it is not missing
    char* lib_name = elf->lib_name;
    if (strcmp(lib_name, OS_LIB) != 0)
    {
        printf_error("Missing library: %s\n", lib_name);
        return false;
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
            return false;
        }
    }

    // Compute GOT runtime address and loaded GOT address
    auto got_runtime_addr = elf->runtime_got_addr;

    // Load libdynlk
    uint proc_num_pages = num_pages;
    if (!load_elf(LIBDYNLK_PATH, SharedObject, nullptr))
        return false;
    auto libdynlk_dep = *elf_dep_list->get(elf_dep_list->size() - 1);
    ELF* libdynlk_elf = libdynlk_dep.elf;

    // Get libdynlk entry point
    void* libdynlk_entry_point = libdynlk_elf->get_libdynlk_main_runtime_addr(proc_num_pages);
    if (libdynlk_entry_point == nullptr)
    {
        printf_error("Error while loading libdynlk");
        return false;
    }

    // Load libc
    if (!load_elf(LIBC_PATH, SharedObject, libdynlk_entry_point))
        return false;

    // GOT setup: GOT[0] unused, GOT[1] = addr of GRUB module (to identify the program), GOT[2] = dynamic linker address
    auto* got_addr = (Elf32_Addr*)runtime_address_to_load_address(got_runtime_addr);
    *(void**)(got_addr + 1) = (void*)elf;
    *(void**)(got_addr + 2) = libdynlk_entry_point;

    // Indicate entry point
    stack_state.eip = elf->global_hdr.e_entry;

    return true;
}

bool ELFLoader::apply_relocations(ELF* elf, uint elf_runtime_load_address) const
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
                uint symbol_id = ELF32_R_SYM(reloc->r_info);
                Elf32_Sym* symbol = &elf->symbols[symbol_id];
                // Weak symbols that are not yet defined are not an issue
                if (ELF32_ST_BIND(symbol->st_info) != STB_WEAK || symbol->st_shndx != 0)
                {
                    printf_error("unsupported R_396_PC32 relocation");
                    return false;
                }
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

    return true;
}

bool ELFLoader::alloc_elf_memory(ELF& elf)
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
            uint page_id = (767 * PT_ENTRIES + PT_ENTRIES - 1 - lib_num_code_pages + runtime_page_id);
            current_process->update_pte(page_id, PTE(Memory::page_tables, pe), true);
        }
    }
    num_pages += lib_num_code_pages;

    return true;
}

void
ELFLoader::copy_elf_subsegment_to_address_space(const void* bytes_ptr, uint n, Elf32_Phdr* h,
                                                uint lib_runtime_load_address,
                                                uint& copied_bytes) const
{
    auto runtime_address = lib_runtime_load_address + (h->p_vaddr + copied_bytes);
    auto dest_addr = runtime_address_to_load_address(runtime_address);
    memcpy((void*)dest_addr, bytes_ptr, n);

    // Update counter
    copied_bytes += n;
}

void ELFLoader::register_elf_init_and_fini(ELF* elf, uint runtime_load_address)
{
    uint init_arr_byte_size = 0;
    uint fini_arr_byte_size = 0;
    Elf32_Addr* init_array = nullptr;
    Elf32_Addr* fini_array = nullptr;
    Elf32_Addr init_runtime_address = 0;
    Elf32_Addr fini_runtime_address = 0;

    // Collect _init, _fini, init_array and fini_array pointer
    for (Elf32_Dyn* d = elf->dyn_table; d->d_tag != DT_NULL; d++)
    {
        switch (d->d_tag)
        {
            case DT_INIT:
                init_runtime_address = runtime_load_address + d->d_un.d_val;
                break;
            case DT_FINI:
                fini_runtime_address = runtime_load_address + d->d_un.d_val;
                break;
            case DT_INIT_ARRAY:
            {
                Elf32_Addr runtime_address = runtime_load_address + d->d_un.d_val;
                init_array = (Elf32_Addr*)runtime_address_to_load_address(runtime_address);
                break;
            }
            case DT_FINI_ARRAY:
            {
                Elf32_Addr runtime_address = runtime_load_address + d->d_un.d_val;
                fini_array = (Elf32_Addr*)runtime_address_to_load_address(runtime_address);
                break;
            }
            case DT_INIT_ARRAYSZ:
                init_arr_byte_size = d->d_un.d_val;
                break;
            case DT_FINI_ARRAYSZ:
                fini_arr_byte_size = d->d_un.d_val;
                break;
            default:
                break;
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
            uint runtime_page_id = runtime_address >> 12;
            uint page_id = runtime_address_to_load_address(runtime_address) >> 12;
            uint pde = runtime_page_id / PT_ENTRIES;
            uint pte = runtime_page_id % PT_ENTRIES;
            page_tables[pde].entries[pte] = PTE(current_process->page_tables, page_id);

            bool no_write = !(h->p_flags & PF_W); // Segment has no write permissions
            if (no_write)
                page_tables[pde].entries[pte] &= ~PAGE_WRITE;
        }
    }
}

bool ELFLoader::load_elf(const char* path, ELF_type expected_type, void* lib_dynlk_runtime_entry_point)
{
    auto buf = VFS::load_file(path);
    if (!buf)
        return false;
    ELF* elf;
    if (!((elf = ELF::is_valid((uint)buf, expected_type))))
    {
        delete[] (char*)buf;
        return false;
    }

    uint load_address = (uint)buf;
    uint runtime_load_addr = num_pages * PAGE_SIZE;

    alloc_elf_memory(*elf);
    map_elf(elf, runtime_load_addr);

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
            copy_elf_subsegment_to_address_space(bytes_ptr, num_first_bytes, h, runtime_load_addr, copied_bytes);
        }

        // Copy whole pages
        while (copied_bytes + PAGE_SIZE < h->p_filesz)
        {
            bytes_ptr = (void*)(load_address + h->p_offset + copied_bytes);
            copy_elf_subsegment_to_address_space(bytes_ptr, PAGE_SIZE, h, runtime_load_addr, copied_bytes);
        }

        // Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
        uint num_final_bytes = h->p_filesz - copied_bytes;
        if (num_final_bytes)
        {
            bytes_ptr = (void*)(load_address + h->p_offset + copied_bytes);
            copy_elf_subsegment_to_address_space(bytes_ptr, num_final_bytes, h, runtime_load_addr, copied_bytes);
        }

        // Fill the rest with 0. Very important, this is where bss is.
        if (h->p_memsz > h->p_filesz)
        {
            auto runtime_address = runtime_load_addr + (h->p_vaddr + h->p_filesz);
            auto dest_addr = runtime_address_to_load_address(runtime_address);
            memset((void*)dest_addr, 0, h->p_memsz - h->p_filesz);
        }
    }

    if (!apply_relocations(elf, runtime_load_addr))
    {
        delete[] (char*)buf;
        return false;
    }

    register_elf_init_and_fini(elf, runtime_load_addr);

    if (expected_type == SharedObject && lib_dynlk_runtime_entry_point)
    {
        auto got_runtime_addr = runtime_load_addr + elf->runtime_got_addr;

        //printf("%s GOT: 0x%x\n", path, got_runtime_addr);

        // Write GOT entries 1 and 2 (unique ID and libdynlk runtime entry point)
        auto got_load_addr = (Elf32_Addr*)runtime_address_to_load_address(got_runtime_addr);
        *(void**)(got_load_addr + 1) = (void*)elf;
        *(void**)(got_load_addr + 2) = lib_dynlk_runtime_entry_point;
    }

    elf_dep_list->add({elf, runtime_load_addr});
    delete[] (char*)buf;

    return true;
}

Process* ELFLoader::build_process(int argc, const char** argv, pid_t pid, pid_t ppid,
                                  const char* path, uint priority)
{
    if (!load_elf(path, Executable, nullptr))
        return nullptr;

    ELF* main_elf = elf_dep_list->get(0)->elf;
    if (main_elf->interpreter_name != nullptr && !dynamic_loading(main_elf))
        return nullptr;

    auto k_stack_top = finalize_process_setup(argc, argv);

    used = true;
    return new Process(num_pages, elf_dep_list, page_tables, pdt, &stack_state, priority, pid, ppid, k_stack_top);
}

Elf32_Addr ELFLoader::finalize_process_setup(int argc, const char** argv)
{
    if (page_tables[767].entries[1023] != 0)
        printf_error("Process kernel page entry is not empty");
    // Allocate process stack page
    uint p_stack_pe_id = Memory::get_free_pe_user();
    Memory::allocate_page_user<false>(p_stack_pe_id);

    // Map it in current process' address space
    uint p_stack_page_id = 767 * PT_ENTRIES + PT_ENTRIES - 1 - num_pages - 1;
    uint p_stack_pde = p_stack_page_id / PT_ENTRIES;
    uint p_stack_pte = p_stack_page_id % PT_ENTRIES;
    current_process->page_tables[p_stack_pde].entries[p_stack_pte] = PTE(Memory::page_tables, p_stack_pe_id);
    INVALIDATE_PAGE(p_stack_pde, p_stack_pte);

    // Compute stack top
    Elf32_Addr p_stack_bottom_v_addr = VIRT_ADDR(p_stack_pde, p_stack_pte, 0);
    Elf32_Addr p_stack_top_v_addr = p_stack_bottom_v_addr + PAGE_SIZE;

    // Allocate syscall handler stack page
    uint k_stack_pe = Memory::get_free_pe();
    uint k_stack_pde = k_stack_pe / PDT_ENTRIES;
    uint k_stack_pte = k_stack_pe % PDT_ENTRIES;
    Memory::allocate_page<false>(k_stack_pe);

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
        pdt->entries[i] = PHYS_ADDR(Memory::page_tables, (uint) &Memory::page_tables[i]) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;
    // Todo: remove PAGE_USER ? Seems impossible while allocate_page_user allocates above KERNEL_VIRTUAL_BASE

    // Map process stack at 0xBFFFFFFC = 0xCFFFFFFF - 4 at pde 767 and pte 1023, just below the kernel
    if (page_tables[767].entries[1023] != 0)
        printf_error("Process kernel page entry is not empty");
    page_tables[767].entries[1023] = PTE(Memory::page_tables, p_stack_pe_id);
    pdt->entries[767] = PHYS_ADDR(Memory::page_tables, (uint) &page_tables[767]) | PAGE_USER | PAGE_WRITE |
            PAGE_PRESENT;

    // Setup PCB
    //proc->stack_state.eip = 0;
    stack_state.ss = 0x20 | 0x03;
    stack_state.cs = 0x18 | 0x03;
    stack_state.esp = write_args_to_stack(p_stack_top_v_addr, argc, argv, init_fini.init_array,
                                                init_fini.fini_array);
    stack_state.eflags = 0x200;
    stack_state.error_code = 0;

    // Unmap new process from current process' address space
    for (size_t i = 0; i < num_pages + 1; i++) // +1 for stack
    {
        uint page_id = 767 * PT_ENTRIES + PT_ENTRIES - 2 - i;
        uint pde = page_id / PT_ENTRIES;
        uint pte = page_id % PT_ENTRIES;
        current_process->page_tables[pde].entries[pte] = 0;
        INVALIDATE_PAGE(pde, pte);
    }

    return VIRT_ADDR(k_stack_pde, k_stack_pte, (PAGE_SIZE - sizeof(int)));
}

const char** ELFLoader::add_argv0_to_argv(const char** argv, const char* path, int& argc)
{
    argc++;
    const char** new_argv = new const char*[argc];
    memcpy(new_argv + 1, argv, (argc - 1) * sizeof(char*));
    const char* abs_path = VFS::get_absolute_path(path);
    if (abs_path)
        new_argv[0] = abs_path;
    else
    {
        delete[] new_argv;
        return nullptr;
    }
    return new_argv;
}

Elf32_Addr ELFLoader::runtime_address_to_load_address(Elf32_Addr runtime_address) const
{
    uint page_sub = num_pages - (runtime_address >> 12);
    uint pde = (767 - page_sub / PT_ENTRIES);
    uint pte = (PT_ENTRIES - 1) - page_sub % PT_ENTRIES;
    uint off = runtime_address % PAGE_SIZE;

    return VIRT_ADDR(pde, pte, off);
}

void ELFLoader::offset_memory_mapping(uint offset) const
{
    // Offset pages
    for (size_t i = 0; i < num_pages; i++)
    {
        uint page_id = 767 * PT_ENTRIES + PT_ENTRIES - 1 - num_pages + i;
        uint new_page_id = page_id - offset;

        current_process->update_pte(new_page_id, PTE(current_process->page_tables, page_id), true);
    }
    // Zero out 'offset' pages on the right
    for (size_t i = 0; i < offset; i++)
    {
        uint page_id = 767 * PT_ENTRIES + PT_ENTRIES - 1 - offset + i;
        current_process->update_pte(page_id, 0, true);
    }
}

Process* ELFLoader::setup_elf_process(pid_t pid, pid_t ppid, int argc, const char** argv,
                                      const char* path, uint priority)
{
    ELFLoader loader{};

    // Add argv0 to argv. This can fail if the resolution of the absolute path fails
    const char** full_argv;
    if (!((full_argv = add_argv0_to_argv(argv, path, argc))))
        return nullptr;

    Process* proc = loader.build_process(argc, full_argv, pid, ppid, path, priority);

    delete full_argv[0]; // delete argv[0]
    delete[] full_argv;

    return proc;
}

size_t ELFLoader::write_args_to_stack(size_t stack_top_v_addr, int argc, const char** argv, list<Elf32_Addr>
                                      init_array, list<Elf32_Addr> fini_array)
{
    size_t args_len = 0; // Actual size of all args combined
    for (int i = 0; i < argc; ++i)
        args_len += strlen(argv[i]) + 1;
    // Word aligned total args size
    size_t args_occupied_space =
        args_len & (sizeof(int) - 1) ? (args_len & ~(sizeof(int) - 1)) + sizeof(int) : args_len;

    char* stack_top = (char*)stack_top_v_addr;
    char** argv_ptr = (char**)((uint)(stack_top - args_occupied_space));
    for (int i = argc - 1; i >= 0; --i)
    {
        size_t l = strlen(argv[i]) + 1; // Current arg len
        stack_top -= l; // Point to beginning of string
        argv_ptr -= 1; // Point to argv[i] location
        *argv_ptr = (char*)(KERNEL_VIRTUAL_BASE - ((char*)stack_top_v_addr - stack_top)); // Write argv[i]
        memcpy(stack_top, argv[i], l); // Write ith string
    }
    char* argv0_addr = (char*)KERNEL_VIRTUAL_BASE - ((char*)stack_top_v_addr - (char*)argv_ptr);

    // Compute end of stack pointer, pointing at argc, then argv, then init and fini arrays, both null terminated
    uint num_funcs = init_array.size() + fini_array.size() + 2;
    argv_ptr = (char**)((uint)argv_ptr - num_funcs * sizeof(Elf32_Addr) - sizeof(void*) - sizeof(uint));
    argv_ptr = (char**)((uint)argv_ptr & ~0xF); // ABI 16 bytes alignment
    *(argv_ptr + 1) = argv0_addr; // Argv
    *(uint*)(argv_ptr) = argc; // Argc

    // Write init and fini arrays, null terminated
    Elf32_Addr* funcs_ptr = (Elf32_Addr*)(argv_ptr + 2);
    size_t init_arr_size = init_array.size();
    for (size_t i = 0; i < init_arr_size; ++i)
        *funcs_ptr++ = *init_array.get(i);
    *funcs_ptr++ = 0;
    size_t fini_arr_size = fini_array.size();
    for (size_t i = 0; i < fini_arr_size; ++i)
        *funcs_ptr++ = *fini_array.get(i);
    *funcs_ptr++ = 0;

    size_t esp = KERNEL_VIRTUAL_BASE - ((char*)stack_top_v_addr - (char*)argv_ptr);
    return esp;
}
