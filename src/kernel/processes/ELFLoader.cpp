#include "ELFLoader.h"

#include "../core/fb.h"
#include "../file_management/VFS.h"
#include <kstring.h>

#include "scheduler.h"
#include "abi-bits/fcntl.h"
#include "abi-bits/vm-flags.h"

using namespace ELFTools;

ELFLoader::ELFLoader(): current_process(Scheduler::get_running_process()), elf_dep_list(new list<elf_dependence>),
                        page_tables((Memory::page_table_t*)Memory::calloca(768, sizeof(Memory::page_table_t))),
                        pdt((Memory::pdt_t*)Memory::malloca(sizeof(Memory::pdt_t)))
{
}

ELFLoader::~ELFLoader()
{
    // Free stuff not transferred to process
    for (auto i = 0; i < elf_dep_list->size(); i++)
        delete elf_dep_list->get(i)->elf;
    delete elf_dep_list;

    if (used)
        return;

    // Process creation failed, so free stuff that should've been transferred to process

    Memory::freea(page_tables);
    Memory::freea(pdt);
}

template <typename ptr_inner_type>
Lptr<ptr_inner_type> ELFLoader::new_lptr(Elf32_Addr runtime_address) const
{
    const auto pptr = static_cast<ptr_inner_type*>(reinterpret_cast<void*>(runtime_address));
    return Lptr<ptr_inner_type>(pptr, &allocations);
}

template <typename ptr_inner_type>
Lptr<ptr_inner_type> ELFLoader::new_lptr(ptr_inner_type runtime_address) const
{
    return Lptr<ptr_inner_type>(runtime_address, &allocations);
}

bool ELFLoader::dynamic_loading(const ELF* elf)
{
    if (elf->interpreter_name == nullptr)
        return true;

    // Load interpreter
    auto interpreter = VFS::browse_to(elf->interpreter_name);
    if (!load_elf(interpreter, SharedObject))
        return false;

    return true;
}

void ELFLoader::map_elf(const ELF* load_elf, Elf32_Addr runtime_load_address)
{
    for (int k = 0; k < load_elf->global_hdr.e_phnum; ++k)
    {
        const Elf32_Phdr* h = &load_elf->prog_hdrs[k];
        if (h->p_type != PT_LOAD)
            continue;

        uint segment_num_pages = ADDR_PAGE(h->p_memsz + PAGE_SIZE - 1);

        Elf32_Addr runtime_address = runtime_load_address + h->p_vaddr;
        uint runtime_page_id = ADDR_PAGE(runtime_address);

        // Allocate page in kernel address space
        int err;
        auto load_addr = (Elf32_Addr)
        Memory::mmap((void*)KERNEL_VIRTUAL_BASE,  segment_num_pages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0, err, Memory::kernel_process, false, true);
        if (!load_addr)
            irrecoverable_error("%s: mmap failed", __func__);

        // Map it in process address space
        for (uint i = 0; i < segment_num_pages; i++)
        {
            if (PTE(page_tables, runtime_page_id + i))
                irrecoverable_error("%s: address space overlap detected", __func__);
            PTE(page_tables, runtime_page_id + i) = PTE(Memory::page_tables, ADDR_PAGE(load_addr) + i) | PAGE_USER;
        }

        // Check if page should have write permissions, and remove the permission if it's not the case
        const bool write = h->p_flags & PF_W;
        if (!write)
            for (uint i = 0; i < segment_num_pages; i++)
                PTE(page_tables, runtime_page_id + i) &= ~PAGE_WRITE;

        // Register allocation
        const auto runtime_addr_base_page = runtime_page_id << 12;
        const int prot = PROT_READ | (write ? PROT_WRITE : 0);
        allocations.add({
            {
                runtime_addr_base_page,
                runtime_addr_base_page + segment_num_pages * PAGE_SIZE,
                Memory::page_info{prot, DEFAULT_U_FLAGS, DEFAULT_U_POLICY}
            },
            load_addr
        });
    }
    num_pages += load_elf->num_pages();
}

void ELFLoader::load_elf_code(const ELF* elf, uint load_address, uint runtime_load_address) const
{
    // Copy lib code in allocated space
    for (int k = 0; k < elf->global_hdr.e_phnum; ++k)
    {
        const Elf32_Phdr* h = &elf->prog_hdrs[k];
        if (h->p_type != PT_LOAD)
            continue;

        const void* bytes_ptr = (void*)(load_address + h->p_offset);

        // Copy segment
        const auto runtime_address = runtime_load_address + h->p_vaddr;
        Lptr dest_addr = new_lptr<void>(runtime_address);
        dest_addr.memcpy(bytes_ptr, h->p_filesz);

        // Fill the rest with 0. Very important, this is where bss is.
        if (h->p_memsz > h->p_filesz)
        {
            dest_addr = runtime_load_address + (h->p_vaddr + h->p_filesz);
            dest_addr.memset(0, h->p_memsz - h->p_filesz);
        }
    }
}

void ELFLoader::allocate_stacks()
{
    int err;
    constexpr int prot = PROT_READ | PROT_WRITE;
    constexpr int flags = MAP_ANONYMOUS | MAP_PRIVATE;

    // Allocate process stack pages
    void* stack_load_address =
            Memory::mmap(nullptr, PROCESS_STACK_SIZE, prot, flags, 0, 0, err, Memory::kernel_process, false, true);
    if (!stack_load_address)
        irrecoverable_error("%s: mmap failed", __func__);
    // Map them in process address space
    constexpr uint stack_start_page_id = 768 * PT_ENTRIES - PROCESS_STACK_N_PAGES;
    for (int i = 0; i < PROCESS_STACK_N_PAGES; i++)
    {
        uint dest_page_id = stack_start_page_id + i;
        if (PTE(page_tables, dest_page_id) != 0)
            irrecoverable_error("%s: Process kernel stack page entry is not empty", __FUNCTION__);
        PTE(page_tables, dest_page_id) = PTE(Memory::page_tables, ADDR_PAGE((uint)stack_load_address) + i); // Todo: use update_pte ?
    }
    allocations.add({{stack_start_page_id << 12, (stack_start_page_id + PROCESS_STACK_N_PAGES) << 12, Memory::DEFAULT_U_PAGE_INFO}, (Elf32_Addr)stack_load_address});

    // Allocate syscall stack pages
    void* kstack_load_address =
            Memory::mmap(nullptr, PROCESS_SYSCALL_STACK_SIZE, prot, flags, 0, 0, err, Memory::kernel_process, false, false);
    if (!kstack_load_address)
        irrecoverable_error("%s: mmap failed", __func__);
    // Allocate syscall handler stack pages
    uint kstack_start_page_id = 768 * PT_ENTRIES - PROCESS_STACK_N_PAGES - PROCESS_SYSCALL_STACK_N_PAGES;
    for (int i = 0; i < PROCESS_SYSCALL_STACK_N_PAGES; i++)
    {
        uint kstack_dest_page_id = kstack_start_page_id + i;
        if (PTE(page_tables, kstack_dest_page_id) != 0)
            irrecoverable_error("%s: Process kernel stack page entry is not empty", __FUNCTION__);
        PTE(page_tables, kstack_dest_page_id) = PTE(Memory::page_tables, ADDR_PAGE((uint)kstack_load_address) + i);
    }
    allocations.add({{kstack_start_page_id << 12, (kstack_start_page_id + PROCESS_SYSCALL_STACK_N_PAGES) << 12, Memory::DEFAULT_K_PAGE_INFO}, (Elf32_Addr)kstack_load_address});

    // Update relevant PDT entries
    for (int i = 0; i < (PROCESS_N_STACKS_PAGES + PT_ENTRIES - 1) / PT_ENTRIES; i++)
        pdt->entries[767 - i] = PHYS_ADDR(Memory::page_tables, (uint) &page_tables[767 - i]) | PAGE_USER | PAGE_WRITE |
                PAGE_PRESENT;
}

void ELFLoader::setup_pcb(int argc, const char** argv, const char** envp)
{
    // elf_dep_list[0] = main, [1] = interpreter (it'd be great to write a proper system to manage that...)
    const auto main_elf = elf_dep_list->get(0)->elf;
    const auto interp_elf_dep = elf_dep_list->get(1);
    const Elf32_Addr entry_point = main_elf->interpreter_name
        ? interp_elf_dep->elf->global_hdr.e_entry + interp_elf_dep->runtime_load_address
        : main_elf->global_hdr.e_entry;
    stack_state.eip = entry_point;
    stack_state.ss = 0x20 | 0x03;
    stack_state.cs = 0x18 | 0x03;
    stack_state.esp = (size_t)write_args_to_stack(argc, argv, envp);
    stack_state.eflags = 0x200;
    stack_state.error_code = 0;
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

Lptr<char> ELFLoader::write_auxv(const Lptr<char>& stack_top, void* const argv0_runtime_addr,
                                            void* const random_runtime_addr) const
{
    const ELF& elf = *elf_dep_list->get(0)->elf;
    auto lt = stack_top.convert_to<auxv_t>();

    // The last entry is AT_NULL to indicate the end of the vector
    (--lt).write(auxv_t{AT_NULL, (long)0});

    (--lt).write(auxv_t{AT_PAGESZ, PAGE_SIZE});

    (--lt).write(auxv_t{AT_SECURE, (long)0});

    (--lt).write(auxv_t{AT_RANDOM,  random_runtime_addr});

    if (const bool has_interpreter = elf.interpreter_name; !has_interpreter)
        return lt.convert_to<char>();

    Elf32_Addr phdr_runtime_addr = ELF32_ADDR_ERR;
    for (int i = 0; i < elf.global_hdr.e_phnum; ++i)
    {
        Elf32_Phdr* h = &elf.prog_hdrs[i];
        if (h->p_type != PT_PHDR)
            continue;
        phdr_runtime_addr = h->p_vaddr;
    }
    if (phdr_runtime_addr == ELF32_ADDR_ERR)
        irrecoverable_error("%s: no PHDR found in program", __func__);
    (--lt).write(auxv_t{AT_PHDR, (long)phdr_runtime_addr});

    (--lt).write(auxv_t{AT_PHENT, elf.global_hdr.e_phentsize});

    (--lt).write(auxv_t{AT_PHNUM, elf.global_hdr.e_phnum});

    (--lt).write(auxv_t{AT_EXECFN, argv0_runtime_addr});

    (--lt).write(auxv_t{AT_ENTRY, (long)elf.global_hdr.e_entry});

    return lt.convert_to<char>();
}

ELF* ELFLoader::load_elf(const SharedPointer<Dentry>& file, ELF_type expected_type)
{
    const auto proc = Scheduler::get_running_process();
    const int fd = proc->open(file->get_absolute_path(), O_RDONLY, 0777);
    if (fd < 0)
        return nullptr;
    const auto buf = new char[file->inode->size];
    if (proc->read(fd, buf, file->inode->size) < 0) // Read file with read (and not VFS::load_file) to benefit from preload
    {
        proc->close(fd);
        return nullptr;
    }

    const auto elf = load_elf(buf, expected_type);
    delete[] buf;

    return elf;
}

ELF* ELFLoader::load_elf(void* buf, ELF_type expected_type)
{
    ELF* elf;
    if (!((elf = ELF::is_valid((uint)buf, expected_type))))
        return nullptr;

    uint load_address = (uint)buf;
    uint runtime_load_addr = num_pages * PAGE_SIZE;
    const auto dep = elf_dependence({elf, runtime_load_addr});
    elf_dep_list->add(dep);

    map_elf(elf, runtime_load_addr);
    load_elf_code(elf, load_address, runtime_load_addr);
    if (!dynamic_loading(elf))
        return nullptr;

    return elf;
}

Process* ELFLoader::build_process(int argc, const char** argv, pid_t pid, pid_t ppid,
                                  const char** envp, const SharedPointer<Dentry>& file, uint priority)
{
    if (!load_elf(file, Executable))
        return nullptr;

    finalize_process_setup(argc, argv, envp);
    constexpr auto k_stack_top = ((768 * PT_ENTRIES - PROCESS_STACK_N_PAGES) << 12) - sizeof(int);

    used = true;
    Process* p = new Process(file->get_absolute_path(), num_pages, page_tables, pdt, &stack_state, priority, pid, ppid, k_stack_top);

    for (const auto& [alloc, _] : allocations)
        p->memtree.register_external_allocation(alloc);

    return p;
}

void ELFLoader::write_to_stack(Lptr<char>& stack_top, const void* content, size_t size)
{
    stack_top -= size;
    stack_top.memcpy(content, size);
}

void ELFLoader::finalize_process_setup(int argc, const char** argv, const char** envp)
{
    setup_pdt();
    allocate_stacks();
    setup_pcb(argc, argv, envp);
}

Process* ELFLoader::setup_elf_process(pid_t pid, pid_t ppid, int argc, const char** argv,
                                      const char** envp, const SharedPointer<Dentry>& file, uint priority)
{
    ELFLoader loader{};
    Process* proc = loader.build_process(argc, argv, pid, ppid, envp, file, priority);

    return proc;
}

void* ELFLoader::write_args_to_stack(int argc, const char** argv, const char** envp) const
{
    // CF https://uclibc.org/docs/psABI-i386.pdf section 2.11
    constexpr uintptr_t stack_end_runtime_addr = KERNEL_VIRTUAL_BASE - sizeof(int);
    auto stack_top = new_lptr<char>(stack_end_runtime_addr);

    constexpr char nulls[4]{};

    if (argc != 0 && argv[argc - 1] == nullptr)
        argc--;

    // Write argv contents and argv array
    for (int i = argc - 1; i >= 0; i--)
        write_to_stack(stack_top, argv[i], strlen(argv[i]) + 1);
    void* const argv0_runtime_addr = stack_top.get_runtime_ptr();

    // Write envp content
    int envpc = 0;
    for (; envp[envpc]; envpc++){};
    void* const last_envp_trailing_address = stack_top.get_runtime_ptr();
    for (int envp_idx = envpc - 1; envp_idx >= 0; envp_idx--)
        write_to_stack(stack_top, envp[envp_idx], strlen(envp[envp_idx]) + 1);

    // Pseudo random bytes generation using rdtsc
    char random[4][4]{};
    for (auto & num : random)
    {
        auto v = rdtsc();
        for (char & byte : num)
        {
            byte = (char)(v & 0xFF);
            v >>= 8;
        }
    }
    write_to_stack(stack_top, random, sizeof(random));
    void* const random_runtime_addr = stack_top.get_runtime_ptr();

    // auxv
    stack_top = write_auxv(stack_top, argv0_runtime_addr, random_runtime_addr);

    // 4 zeros
    write_to_stack(stack_top, nulls, sizeof(nulls));

    // Environment pointers
    char* envp_ptr = (char*)last_envp_trailing_address;
    for (int envp_idx = envpc - 1; envp_idx >= 0; envp_idx--)
    {
        envp_ptr -= strlen(envp[envp_idx]) + 1;
        write_to_stack(stack_top, &envp_ptr, sizeof(envp_ptr));
    }

    // 4 zeroes | Argv trailing zeroes
    write_to_stack(stack_top, nulls, sizeof(nulls));

    // Argv
    char* argv_ptr = (char*)stack_end_runtime_addr;
    for (int i = argc - 1; i >= 0; i--)
    {
        argv_ptr -= strlen(argv[i]) + 1;
        write_to_stack(stack_top, &argv_ptr, sizeof(argv_ptr));
    }

    // argc
    write_to_stack(stack_top, &argc, sizeof(argc));
    return stack_top.get_runtime_ptr();
}
