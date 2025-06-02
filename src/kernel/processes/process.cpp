#include "process.h"

#include <kstring.h>

#include "ELFLoader.h"
#include "scheduler.h"
#include "../core/memory.h"
#include "../core/fb.h"
#include "../file_management/VFS.h"
#include "../utils/comparison.h"
#include "sys/errno.h"

list<Process::env_var*> Process::env_list = {};

/** Change current pdt */
extern "C" void change_pdt_asm_(uint pdt_phys_addr);

/*Process* Process::from_binary(GRUB_module* module)
{
    uint mod_size = module->size;
    uint mod_code_pages = mod_size / PAGE_SIZE + ((mod_size % PAGE_SIZE) ? 1 : 0); // Num pages code spans over

    // Allocate process memory
    Process* proc = new Process();
    proc->stack_state.eip = 0;
    proc->num_pages = mod_code_pages;
    proc->pte = (uint*)malloc(mod_code_pages * sizeof(uint));

    // Allocate process code pages
    for (uint k = 0; k < mod_code_pages; ++k)
    {
        uint pe = get_free_pe();
        proc->pte[k] = pe;
        allocate_page_user(get_free_page(), pe);
    }


    uint proc_pe_id = 0;
    uint sys_pe_id = proc->pte[proc_pe_id];
    uint code_page_start_addr = module->start_addr + proc_pe_id * PAGE_SIZE;
    uint dest_page_start_addr =
        (sys_pe_id / PDT_ENTRIES) << 22 | (sys_pe_id % PDT_ENTRIES) << 12;

    // Copy whole pages
    for (; proc_pe_id < mod_size / PAGE_SIZE; ++proc_pe_id)
    {
        code_page_start_addr = module->start_addr + proc_pe_id * PAGE_SIZE;
        sys_pe_id = proc->pte[proc_pe_id];
        dest_page_start_addr = (sys_pe_id / PDT_ENTRIES) << 22 | (sys_pe_id % PDT_ENTRIES) << 12;
        memcpy((void*)dest_page_start_addr, (void*)(module->start_addr + code_page_start_addr),
               PAGE_SIZE);
    }
    // Handle data that does not fit a whole page. First copy the remaining bytes then fill the rest with 0
    memcpy((void*)dest_page_start_addr, (void*)code_page_start_addr, mod_size % PAGE_SIZE);
    memset((void*)(dest_page_start_addr + mod_size % PAGE_SIZE), 0, PAGE_SIZE - mod_size % PAGE_SIZE);

    return proc;
}*/

Process::~Process()
{
    if (!(flags & P_TERMINATED))
    {
        printf_error("Process %u is not terminated", pid);
        return;
    }

    // Free process code
    for (uint i = 0; i < num_pages; ++i)
    {
        if (PTE(page_tables, i))
            Memory::free_page(i * PAGE_SIZE, this);
    }

    // Clear memory leaks
    // for (const auto alloc : allocations)
    //     free(alloc);

    // Close open file descriptors
    for (const auto& file_desc : file_descriptors)
    {
        if (!file_desc)
            continue;
        close(file_desc->fd);
    }

    children.clear();

    delete elf_dep_list;

    Memory::freea(page_tables);
    Memory::freea(pdt);

    flags |= P_TERMINATED;

    //printf_info("Process %u exited with code %d", pid, ret_val);
}

Process::Process(uint num_pages, list<elf_dependence>* elf_dep_list, Memory::page_table_t* page_tables,
    Memory::pdt_t* pdt, stack_state_t* stack_state, uint priority, pid_t pid, pid_t ppid, Elf32_Addr k_stack_top) :
    quantum(0), priority(priority),
    num_pages(num_pages),
    pid(pid), ppid(ppid), k_stack_top(k_stack_top), flags(P_READY), lowest_free_pe(num_pages),
    elf_dep_list(elf_dep_list),
    page_tables(page_tables),
    pdt(pdt),
    program_break(num_pages * PAGE_SIZE)
{
    memcpy(&this->stack_state, stack_state, sizeof(stack_state_t));
}


void Process::terminate(int ret_val)
{
    flags |= P_TERMINATED;
    this->ret_val = ret_val;
}

void* Process::malloc(uint n)
{
    auto alloc = ::malloc<false>(n, this);
    if (alloc)
        allocations.add(alloc);

    return alloc;
}

void* Process::calloc(size_t nmemb, size_t size)
{
    auto alloc= ::calloc(nmemb, size, this);
    if (alloc)
        allocations.add(alloc);

    return alloc;
}

void* Process::realloc(void* ptr, size_t size)
{
    auto alloc = ::realloc(ptr, size, this);
    if (alloc == ptr)
        return alloc;
    if (alloc)
        allocations.add(alloc);

    return alloc;
}

void Process::free(void* ptr)
{
    ::free(ptr, this);

    if (ptr)
        allocations.remove(ptr);
}

pid_t Process::get_pid() const
{
    return pid;
}

void Process::set_flag(uint flag)
{
    flags |= flag;
}

bool Process::is_terminated() const
{
    return flags & P_TERMINATED;
}

bool Process::is_waiting_key() const
{
    return flags & P_WAITING_KEY;
}

bool Process::is_waiting_program() const
{
    return flags & P_WAITING_PROCESS;
}

bool Process::is_sleeping() const
{
    return flags & P_SLEEPING;
}

bool Process::exec_running() const
{
    return flags & P_EXEC;
}

uint Process::get_symbol_runtime_address_at_runtime(uint dep_id, const char* symbol_name) const
{
    for (int i = dep_id; i < elf_dep_list->size(); i++)
    {
        auto dep = elf_dep_list->get(i);
        auto s = dep->elf->get_symbol(symbol_name, dep->runtime_load_address);
        if (s && s->st_shndx && ELF32_ST_BIND(s->st_info) == STB_GLOBAL)
            return dep->runtime_load_address + s->st_value;
    }

    return 0x00;
}

void Process::init()
{
    auto pwd = new env_var{};
    pwd->name = strdup("PWD");
    pwd->value = strdup("/");
    env_list.add(pwd);

    auto oldpwd = new env_var{};
    oldpwd->name = strdup("OLDPWD");
    oldpwd->value = strdup("/");

    auto home = new env_var{};
    home->name = strdup("HOME");
    home->value = strdup(" \t\n");
    env_list.add(home);
}

char* Process::get_env(const char* name)
{
    for (const auto& e : env_list)
    {
        if (strcmp(name, e->name) == 0)
            return e->value;
    }

    return nullptr;
}

void Process::set_env(const char* name, const char* value)
{
    for (const auto& e : env_list)
    {
        if (strcmp(name, e->name) == 0)
        {
            delete e->value;
            e->value = strdup(value);
            return;
        }
    }

    env_list.add(new env_var{strdup(name), strdup(value)});
}

void Process::copy_page_to_other_process(const Process* other, uint page_id, uint mapping_page_id) const
{
    if (!PTE(page_tables, page_id))
        return;

    // Allocate a page in kernel address space
    uint sys_pe = Memory::get_free_pe_user(); // Get sys PTE id
    uint frame = Memory::get_free_frame();
    Memory::allocate_page_user<false>(frame, sys_pe); // Allocate page in kernel address space

    // Register page in child address space
    auto pte_flags = PTE(page_tables, page_id) & 0x7FF;
    auto frame_val = frame << 12;
    other->update_pte(page_id , frame_val | pte_flags, false);

    // Map the new page in current process address space to be able to access it
    update_pte(mapping_page_id, PTE(Memory::page_tables, sys_pe), true);

    // Copy page to child page
    memcpy((void*)(mapping_page_id << 12), (void*)(page_id << 12), PAGE_SIZE);
}

void Process::copy_page_to_other_process_shared(const Process* other, uint page_id) const
{
    auto pte = PTE(page_tables, page_id);
    if (!(pte & PAGE_PRESENT))
        return;

    // Remove write permission in current process, as we cannot write to the page anymore since it is shared
    // Mark page as COW in current and other process
    bool write = pte & PAGE_WRITE;
    update_pte(page_id, (pte | (write ? PAGE_COW : PAGE_SHRO)) & ~PAGE_WRITE, true);
    other->update_pte(page_id , (pte | PAGE_COW) & ~PAGE_WRITE, false);
}

pid_t Process::fork()
{
    auto child_pid = Scheduler::get_free_pid();
    if (child_pid == -1)
        return -1;

    // Allocate PDT and page tables
    auto child_page_tables = (Memory::page_table_t*)Memory::calloca(768, sizeof(Memory::page_table_t));
    auto child_pdt = (Memory::pdt_t*)Memory::malloca(sizeof(Memory::pdt_t));

    // Duplicate dependency list
    auto dep_list = new list<elf_dependence>{};
    for (const auto& dep : *elf_dep_list)
        dep_list->add(dep);

    // Creat child process
    auto child = new Process(num_pages, dep_list, child_page_tables, child_pdt, &stack_state, priority, child_pid,
        pid, k_stack_top);

    // Copy PCB
    child->cpu_state = cpu_state;
    child->k_stack_state = k_stack_state;
    child->k_cpu_state = k_cpu_state;
    child->quantum = quantum;
    child->flags = flags & ~P_SYSCALL_INTERRUPTED;

    // Duplicate page table entries
    uint used_pages = ADDR_PAGE(program_break + PAGE_SIZE);
    uint mapping_page = used_pages; // Free page that will be used to map the child pages in the current address space
    for (uint i = 0; i < used_pages; i++)
        copy_page_to_other_process_shared(child, i);

    // Duplicate stacks - do not use COW for stacks, because they are not shared
    copy_page_to_other_process(child, ADDR_PAGE(KERNEL_VIRTUAL_BASE - PAGE_SIZE), mapping_page);
    copy_page_to_other_process(child, ADDR_PAGE(KERNEL_VIRTUAL_BASE - PAGE_SIZE * 2), mapping_page);

    // Duplicate PDT entries - This MUST be done after copying the pages, because page tables are lazily allocated.
    // If we do it before, the page tables would not be actually allocated yet, thus PHYS_ADDR would return 0
    for (uint i = 0; i < 768; i++)
    {
        if (pdt->entries[i] == 0)
        {
            child->pdt->entries[i] = 0;
            continue;
        }

        auto flags = pdt->entries[i] & 0x7FF;
        auto frame_val = PHYS_ADDR(Memory::page_tables, (uint) &child->page_tables[i]);
        child->pdt->entries[i] = frame_val | flags;
    }
    memcpy(child_pdt->entries + 768, pdt->entries + 768, sizeof(uint) * (PDT_ENTRIES - 768));

    // Clear mapping page
    update_pte(mapping_page, 0, true);

    Scheduler::set_process_ready(child);

    child->cpu_state.eax = 0; // Fork returns 0 in child process
    return child->pid;
}

void Process::update_pte(uint pte, uint val, bool update_cache) const
{
    // Decrease previously mapped frame reference count if there were one being referenced
    uint previous_frame = PTE(page_tables, pte) >> 12;
    if (Memory::frame_rc[previous_frame])
        Memory::frame_rc[previous_frame]--;
    else if (PTE(page_tables, pte) & PAGE_PRESENT && !(PTE(page_tables, pte) & PAGE_LAZY_ZERO))
        irrecoverable_error("huh");

    PTE(page_tables, pte) = val;
    uint pde = pte >> 10;
    if (!pdt->entries[pde])
    {
        pdt->entries[pde] = PHYS_ADDR(Memory::page_tables, (uint) &page_tables[pde]) | PAGE_USER | PAGE_WRITE |
            PAGE_PRESENT;
        if (update_cache)
            Memory::reload_cr3_asm();
    }
    else if (update_cache)
        INVALIDATE_PAGE(pde, pte);

    // Increase frame reference count if we are actually mapping a frame
    if (val & PAGE_PRESENT && !(val & PAGE_LAZY_ZERO))
    {
        uint frame_id = val >> 12;
        Memory::frame_rc[frame_id]++;
    }
}

void* Process::sbrk(int increment)
{
    if (increment < 0 && ((uint)-increment > program_break || increment + program_break < PAGE_SIZE * num_pages))
    {
        printf_error("Trying to deallocate too much memory");
        return nullptr;
    }

    if (program_break + increment > KERNEL_VIRTUAL_BASE - PAGE_SIZE)
    {
        printf_error("Trying to allocate too much memory");
        return nullptr;
    }

    uint new_program_break = program_break + increment;
    uint program_break_page_id = ADDR_PAGE(program_break);
    uint new_program_break_page_id = ADDR_PAGE(new_program_break);

    if (increment < 0)
    {
        // Free unused pages
        for (uint i = new_program_break_page_id + 1; i < program_break_page_id; i++)
            Memory::free_page(i << 12, this);
    }
    else
    {
        // Check if page on which current program break lays is allocated, allocates it if it's not the case
        if (!PTE(page_tables, program_break_page_id))
        {
            uint frame = Memory::get_free_frame();
            Memory::allocate_page_user<false>(frame, Memory::get_free_pe());
            update_pte(program_break_page_id, FRAME_ID_ADDR(frame) | PAGE_PRESENT | PAGE_USER | PAGE_WRITE, true);
        }
        // Allocate necessary pages above current program break
        for (uint i = program_break_page_id + 1; i <= new_program_break_page_id; i++)
        {
            uint frame = Memory::get_free_frame();
            Memory::allocate_page_user<false>(frame, Memory::get_free_pe());
            update_pte(i, FRAME_ID_ADDR(frame) | PAGE_PRESENT | PAGE_USER | PAGE_WRITE, true);
        }
    }

    // Update program break
    auto ret = program_break;
    program_break = new_program_break;

    return (void*)ret;
}

void Process::execve_transfer(Process* proc)
{
    proc->flags = flags & ~P_SYSCALL_INTERRUPTED;
    for (const auto& child : children)
        proc->children.add(child);
    for (const auto& val : values_to_write)
        proc->values_to_write.add(val);
    proc->pid = pid;
    exec_replacement = proc;
}

int Process::open(const char* pathname, int flags)
{
    if (lowest_free_fd == MAX_FD_PER_PROCESS)
        return -EMFILE; // No more file descriptors available for current process

    // Get local fd and update lowest_free_fd
    int fd = lowest_free_fd;

    // Actually try to open the file
    int err;
    if (auto file_descriptor = VFS::open(pathname, flags, fd, err))
    {
        // OK
        file_descriptors[fd] = file_descriptor;
        while (file_descriptors[++lowest_free_fd]) {}
        return file_descriptor->fd;
    }

    // Error
    return err;
}

int Process::read(int fd, void* buf, size_t count) const
{
    if (file_descriptors[fd] == nullptr)
        return -EBADF; // File descriptor not found

    auto& file_desc = file_descriptors[fd];

    return VFS::read(file_desc->system_fd, count, buf);
}

int Process::close(int fd)
{
    if (!file_descriptors[fd])
        return -EBADF; // File descriptor not found

    if (int err = VFS::close(file_descriptors[fd]->system_fd) < 0)
        return err; // Propagate error from VFS

    file_descriptors[fd] = nullptr; // Mark as closed
    lowest_free_fd = min(lowest_free_fd, fd); // Update lowest free fd

    return 0; // Success
}

int Process::lseek(int fd, int offset, int whence) const
{
    if (!file_descriptors[fd])
        return -EBADF; // File descriptor not found

    return VFS::lseek(file_descriptors[fd]->system_fd, offset, whence);
}
