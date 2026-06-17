#include "process.h"

#include <kstring.h>

#include "ELFLoader.h"
#include "scheduler.h"
#include "../core/memory.h"
#include "../core/fb.h"
#include "../file_management/VFS.h"
#include "../utils/comparison.h"
#include "errno.h"
#include <fcntl.h>

int Process::signal_default_action[HIGHEST_SIGNAL + 1] = {};

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

    pre_free();

    delete bin_path;
    delete[] work_dir;

    //printf_info("Process %u exited with code %d", pid, ret_val);
}

void Process::pre_free()
{
    if (is_pre_freed)
        return;

    // Free process code
    for (uint i = 0; i < num_pages; ++i)
    {
        if (PTE(page_tables, i))
            Memory::free_page(i * PAGE_SIZE, this);
    }

    // Clear memory leaks
    if (!exec_running())
    {
        if (Scheduler::get_process(pid) != this)
        {
            if (allocations.size())
                irrecoverable_error("Pre freeing a process after it was removed from Scheduler::processes."
                                    "See comment for details");
            // When this code runs, the process may be removed from Scheduler::processes
            // Freeing allocations could potentially trigger page faults. Page fault modify page_tables.
            // In order to get page_tables, page fault handler logic will call Scheduler::get_running_process()
            // This will then return either null or another process, leading to crash or undefined behavior
            // Thus, allocations must be cleared in a place where we know that process in still in Scheduler::processes
        }
        free_leaks();
    }

    // Close open file descriptors
    for (const auto& file_desc : file_descriptors)
    {
        if (!file_desc)
            continue;
        close(file_desc->fd);
    }

    for (const auto& sa : oldact)
        delete sa;

    if (children.size() && !exec_running())
        irrecoverable_error("Freeing a process that has children");

    delete elf_dep_list;

    Memory::freea(page_tables);
    Memory::freea(pdt);

    is_pre_freed = true;
}

Process::Process(char* bin_path, uint num_pages, list<elf_dependence>* elf_dep_list, Memory::page_table_t* page_tables,
                 Memory::pdt_t* pdt, stack_state_t* stack_state, uint priority, pid_t pid, pid_t ppid, Elf32_Addr k_stack_top) :
    quantum(0), priority(priority),
    num_pages(num_pages),
    pid(pid), ppid(ppid), k_stack_top(k_stack_top), flags(P_READY),
    lowest_free_pe(num_pages),
    bin_path(bin_path),
    elf_dep_list(elf_dep_list),
    page_tables(page_tables),
    pdt(pdt),
    program_break(num_pages * PAGE_SIZE)
{
    memcpy(&this->stack_state, stack_state, sizeof(stack_state_t));
    memcpy(signal_action, signal_default_action, sizeof(signal_action));

    int tty_opening_err;
    if (FileInterface* stdin = VFS::open_tty(TTY::Target::STDOUT, tty_opening_err))
        file_descriptors[0] = new file_descriptor(0, stdin->fd);
    if (FileInterface* stdout = VFS::open_tty(TTY::Target::STDOUT, tty_opening_err))
        file_descriptors[1] = new file_descriptor(1, stdout->fd);
    if (FileInterface* stderr = VFS::open_tty(TTY::Target::STDERR, tty_opening_err))
        file_descriptors[2] = new file_descriptor(2, stderr->fd);

    for (int i = 2; i >= 0; i--)
        if (file_descriptors[i] == nullptr)
            lowest_free_fd = i;

    [[maybe_unused]] const char* file_name;
    work_dir = bin_path ? VFS::get_file_parent_dentry(bin_path, file_name)->get_absolute_path() : nullptr;

    for (int i = 0; i < HIGHEST_SIGNAL; i++)
        signal_action[i] = SIG_DFL;

    signals_contexts.push({{}, {}, sigset_t{}}); // By default, no signal is blocked
    signal_top_level_block_mask = &signals_contexts.peek()->blocked_mask;
}

int Process::get_free_fd()
{
    if (lowest_free_fd == MAX_FD_PER_PROCESS)
        return -1;


    if (file_descriptors[lowest_free_fd])
        irrecoverable_error("%s: fd lowest_free_id is not free", __func__);
    int ret = lowest_free_fd++; // ++ so that fd is not reserved (of course this is not concurrency compliant...)
    while (lowest_free_fd < MAX_FD_PER_PROCESS && file_descriptors[lowest_free_fd]) lowest_free_fd++;

    return ret;
}

void Process::release_fd(int fd)
{
    delete file_descriptors[fd];
    file_descriptors[fd] = nullptr;
    lowest_free_fd = min(lowest_free_fd, fd);
}


void Process::terminate_with_value(int ret_val)
{
    flags |= P_TERMINATED;
    this->ret_status = (ret_val & 0xFF) << 8; // Cf. wait.h
}

void Process::terminate_with_signal(int ret_sig)
{
    flags |= P_TERMINATED;
    this->ret_status = ret_sig & 0x7F; // Cf. wait.h
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

    // Realloc either returns the same ptr, either calls process->malloc so there is no new allocation to register
    // if (alloc)
    //     allocations.add(alloc);

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

const char* Process::get_work_dir() const
{
    return work_dir;
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

bool Process::is_waiting_read() const
{
    return flags & P_WAITING_READ;
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
        auto s = dep->elf->get_dynamic_symbol_at_runtime(symbol_name, dep->runtime_load_address);
        if (s && s->st_shndx && ELF32_ST_BIND(s->st_info) == STB_GLOBAL)
            return dep->runtime_load_address + s->st_value;
    }

    return 0x00;
}

void Process::init()
{
    signal_default_action[SIGHUP] = SIGDISP_TERM;
    signal_default_action[SIGINT] = SIGDISP_TERM;
    signal_default_action[SIGQUIT] = SIGDISP_CORE;
    signal_default_action[SIGILL] = SIGDISP_CORE;
    signal_default_action[SIGTRAP] = SIGDISP_CORE;
    signal_default_action[SIGABRT] = SIGDISP_CORE;
    signal_default_action[SIGBUS] = SIGDISP_CORE;
    signal_default_action[SIGFPE] = SIGDISP_CORE;
    signal_default_action[SIGKILL] = SIGDISP_TERM;
    signal_default_action[SIGUSR1] = SIGDISP_TERM;
    signal_default_action[SIGSEGV] = SIGDISP_CORE;
    signal_default_action[SIGUSR2] = SIGDISP_TERM;
}

void Process::copy_page_to_other_process(const Process* other, uint page_id, uint mapping_page_id) const
{
    if (!PTE(page_tables, page_id))
        return;

    // Allocate a page in kernel address space
    uint sys_pe = Memory::get_free_pe_user(); // Get sys PTE id
    uint frame = Memory::get_free_frame();
    Memory::allocate_page_user<false>(frame, sys_pe, true); // Allocate page in kernel address space

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
    const bool write = pte & PAGE_WRITE || pte & PAGE_COW;
    const uint pte_val = (pte | (write ? PAGE_COW : PAGE_SHRO)) & ~PAGE_WRITE;
    update_pte(page_id, pte_val, true);
    other->update_pte(page_id , pte_val, false);
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
    auto child = new Process(strdup(bin_path), num_pages, dep_list, child_page_tables, child_pdt, &stack_state, priority, child_pid,
        pid, k_stack_top);

    // Copy PCB
    child->cpu_state = cpu_state;
    child->k_stack_state = k_stack_state;
    child->k_cpu_state = k_cpu_state;
    child->quantum = quantum;
    child->flags = flags & ~P_SYSCALL_INTERRUPTED;
    child->tls_base = tls_base;

    // Duplicate page table entries
    uint mapping_page = ADDR_PAGE(KERNEL_VIRTUAL_BASE) - PROCESS_N_STACKS_PAGES - 1; // Free page that will be used to map the child pages in the current address space
    if (page_tables[mapping_page / PT_ENTRIES].entries[mapping_page % PT_ENTRIES])
        irrecoverable_error("%s: mapping mage is not empty", __func__);

    uint page_id_off = 0;
    for (uint i = 0; i < 768 - (PROCESS_N_STACKS_PAGES + PT_ENTRIES - 1) / PT_ENTRIES; i++)
    {
        if (pdt->entries[i])
        {
            for (uint j = 0; j < PT_ENTRIES; j++)
            {
                if (page_tables[i].entries[j])
                    copy_page_to_other_process_shared(child, page_id_off + j);
            }
        }
        page_id_off += PT_ENTRIES;
    }

    // Duplicate stacks - do not use COW for stacks, because they are not shared
    for (int i = 0; i <= PROCESS_STACK_N_PAGES; i++) // First copy the process stack
        copy_page_to_other_process(child, ADDR_PAGE(KERNEL_VIRTUAL_BASE - PAGE_SIZE * (i + 1)), mapping_page);
    for (int i = 0; i <= PROCESS_SYSCALL_STACK_N_PAGES; i++) // Next proceed with syscall stack
        copy_page_to_other_process(child, ADDR_PAGE(KERNEL_VIRTUAL_BASE - PAGE_SIZE * (PROCESS_STACK_N_PAGES + i + 1)), mapping_page);

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

    // Copy file descriptors
    for (uint i = 0; i < MAX_FD_PER_PROCESS; i++)
    {
        if (file_descriptors[i] == nullptr)
            continue;
        const file_descriptor* fd = file_descriptors[i];
        file_descriptor*& child_fd = child->file_descriptors[i];
        if (child_fd)
            child->close(child_fd->fd);
        child_fd = new file_descriptor(fd->fd, fd->sys_fd, fd->clo_exec);
    }
    child->lowest_free_fd = lowest_free_fd;

    // Copy signal state
    memcpy(child->signal_action, signal_action, sizeof(signal_action));
    child->pending_signals = {}; // Reset pending signals (cf. man 2 fork)
    child->signals_contexts.peek()->blocked_mask = *signal_top_level_block_mask;
    for (int i = 0 ; i < HIGHEST_SIGNAL + 1; i++)
    {
        if (oldact[i])
            child->oldact[i] = new struct sigaction(*oldact[i]);
    }

    Scheduler::set_process_ready(child);

    children.add(child->pid);
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

void Process::execve_transfer(Process* proc)
{
    proc->flags = flags & ~P_SYSCALL_INTERRUPTED;
    for (const auto& child : children)
        proc->children.add(child);
    for (const auto& val : values_to_write)
        proc->values_to_write.add(val);
    proc->pid = pid;
    exec_replacement = proc;
    proc->is_waiting_for_any_child_to_terminate = is_waiting_for_any_child_to_terminate;
    proc->is_waited_by_parent = is_waited_by_parent;

    // Transfer file descriptors that are not marked to close-on-exec
    for (int i = 0; i < MAX_FD_PER_PROCESS; i++)
    {
        file_descriptor* fd = file_descriptors[i];
        if (fd == nullptr)
            continue;

        if (fd->clo_exec)
            close(fd->fd);
        else
        {
            // Close already opened FD (typically for stdin/out/err)
            if (proc->file_descriptors[i] != nullptr)
                proc->close(i);
            proc->file_descriptors[i] = fd;
            file_descriptors[i] = nullptr;
        }
    }
    // Update proc->lowest_free_fd
    proc->lowest_free_fd = MAX_FD_PER_PROCESS;
    for (int i = MAX_FD_PER_PROCESS - 1; i >= 0; i--)
    {
        if (proc->file_descriptors[i] == nullptr)
            proc->lowest_free_fd = i;
    }

    // Clear memory leaks
    for (const auto& alloc : list(allocations))
        free(alloc);
    allocations.clear();

    // Copy work dir
    free(proc->work_dir);
    proc->work_dir = nullptr;
    if (work_dir)
        proc->work_dir = strdup(work_dir);

    // Copy signal state
    memcpy(proc->signal_action, signal_action, sizeof(signal_action));
    proc->pending_signals = pending_signals;
    proc->signals_contexts.peek()->blocked_mask = *signal_top_level_block_mask;
    for (int i = 0 ; i < HIGHEST_SIGNAL + 1; i++)
    {
        if (oldact[i])
            proc->oldact[i] = new struct sigaction(*oldact[i]);
    }

    // Reset custom signal handlers to default. Ignored or defaulted signals remain untouched (cf man 2 execve)
    for (auto& sighandler : proc->signal_action)
    {
        if (sighandler != SIG_ERR && sighandler != SIG_DFL && sighandler != SIG_IGN)
            sighandler = SIG_DFL;
    }
}

void Process::register_mmap_allocation(const Memory::allocation& allocation)
{
    mmap_allocations.add(allocation);
}

bool Process::deallocate(void* addr, Memory::allocation& alloc)
{
    for (auto it = mmap_allocations.begin(); it != mmap_allocations.end(); ++it)
    {
        if (it->start == (uintptr_t)addr)
        {
            mmap_allocations.remove(*it);
            alloc = *it;
            return true;
        }
    }

    return false;
}

void Process::free_leaks()
{
    const auto allocs = allocations; // Make a copy to avoid modifying while iterating
    for (const auto& alloc : allocs)
        free(alloc);

    if (allocations.size())
        irrecoverable_error("%s: process allocations not empty after freeing all its entries", __func__);
}

int Process::open(const char* pathname, int flags, mode_t mode)
{
    int fd = get_free_fd();
    if (fd == -1)
        return -EMFILE; // No more file descriptors available for current process

    int err;
    if (auto sys_fd = VFS::open_file(pathname, flags, mode, err, work_dir))
    {
        // OK
        file_descriptors[fd] = new file_descriptor {fd, sys_fd->fd};
        return fd;
    }

    // Error
    release_fd(fd);
    return err;
}

int Process::read(int fd, void* buf, size_t count) const
{
    auto sys_fd = proc_to_sys_fd(fd);
    if (sys_fd == -1)
        return -EBADF; // File descriptor not found

    return VFS::read(sys_fd, count, buf);
}

int Process::write(int fd, uint count, void* buf) const
{
    auto sys_fd = proc_to_sys_fd(fd);
    if (sys_fd == -1)
        return -EBADF; // File descriptor not found

    return VFS::write(sys_fd, buf, count);
}

int Process::fstat(int fd, struct stat* statbuf) const
{
    auto sys_fd = proc_to_sys_fd(fd);
    if (sys_fd == -1)
        return -EBADF; // File descriptor not found

    return VFS::fstat(sys_fd, statbuf);
}

int Process::stat(const char* pathname, struct stat* statbuf) const
{
    int open_err;
    FileInterface* sys_fd = VFS::open_file(pathname, O_RDONLY, 0, open_err, work_dir);
    if (sys_fd == nullptr)
        return -open_err;
    int res = VFS::fstat(sys_fd->fd, statbuf);
    VFS::close(sys_fd->fd);
    return res;
}

int Process::close(int fd)
{
    int sys_fd = proc_to_sys_fd(fd);
    if (sys_fd == -1)
        return -EBADF; // File descriptor not found

    release_fd(fd);
    if (int err = VFS::close(sys_fd) < 0)
        return err; // Propagate error from VFS

    return 0; // Success
}

int Process::lseek(int fd, int offset, int whence) const
{
    int sys_fd = proc_to_sys_fd(fd);
    if (sys_fd == -1)
        return -EBADF; // File descriptor not found

    return VFS::lseek(sys_fd, offset, whence);
}

int Process::proc_to_sys_fd(int fd) const
{
    if (fd >= 0 && fd < MAX_FD_PER_PROCESS && file_descriptors[fd])
    {
        FileInterface* sys_fd = VFS::file_descriptors[file_descriptors[fd]->sys_fd];
        if (!sys_fd)
            irrecoverable_error("Dangling process file descriptor");
        return sys_fd->fd; // Return system file descriptor
    }

    return -1;
}

constexpr const char* sig_names[] = {
    "SIGUNUSED",
    "SIGHUP",
    "SIGINT",
    "SIGQUIT",
    "SIGILL",
    "SIGTRAP",
    "SIGABRT",
    "SIGBUS",
    "SIGFPE",
    "SIGKILL",
    "SIGUSR1",
    "SIGSEGV",
    "SIGUSR2",
};

constexpr const char* sig_to_sig_name(int sig)
{
    if (sig >= 0 && sig < (int)(sizeof(sig_names) / sizeof(sig_names[0])))
        return sig_names[sig];
    return "UNKNOWN";
}

int Process::kill(int signal)
{
    if (signal == SIGCANCEL)
    {
        printf_warn("%s: Signal SIGCANCEL is not supported yet, failing", __PRETTY_FUNCTION__);
        kill(SIGQUIT);
        return -1;
    }
    if (signal > HIGHEST_SIGNAL)
    {
        printf_warn("%s: Signal %d is not supported yet, ignoring it", __PRETTY_FUNCTION__, signal);
        return -EINVAL;
    }
    const auto default_action = signal_default_action[signal];
    const bool default_act_is_core = default_action == SIGDISP_CORE;
    const bool default_act_is_term = default_action == SIGDISP_TERM;
    if (signal_action[signal] == SIG_DFL && (default_act_is_core || default_act_is_term))
    {
        printf_warn("Process %d (%s) received signal %s. Exiting.", pid, bin_path, sig_names[signal]);
        terminate_with_signal(signal);
        TRIGGER_TIMER_INTERRUPT
        return 0;
    }
    const auto signo = signal - 1;
    pending_signals.__sig[signo / (8  * sizeof(unsigned long))] |= 1 << (signo % (8 * sizeof(unsigned long)));

    return 0;
}

__sighandler Process::register_signal_handler(int signal, __sighandler handler)
{
    auto prev = signal_action[signal];
    if (handler == SIG_IGN)
        signal_action[signal] = SIG_IGN;
    else if (handler == SIG_DFL)
        signal_action[signal] = SIG_DFL;
    else
        signal_action[signal] = handler;

    return prev;
}

void Process::signal_context_save(int signal)
{
    if (!signals_contexts.push({cpu_state, stack_state, oldact[signal]->sa_mask}))
    {
        printf_warn("%s: maximum number of concurrent signals reached", __PRETTY_FUNCTION__);
        kill(SIGQUIT);
    }
}

void Process::signal_context_restore()
{
    const auto context = signals_contexts.pop();

    if (!context)
        irrecoverable_error("%s: no signal context to restore", __PRETTY_FUNCTION__);
    cpu_state = context->cpu_state;
    stack_state = context->stack_state;
}

int Process::fcntl(int fd, int op, va_list arg) const
{
    int sys_fd = proc_to_sys_fd(fd);
    if (sys_fd == -1)
        return -EBADF; // File descriptor not found

    switch(op)
    {
        case F_GETFL:
            return VFS::file_descriptors[sys_fd]->flags;
        case F_SETFL:
        {
            int flags = va_arg(arg, int);
            // man page indicates only those flags can be affected bby SETFL
            constexpr int flags_mask = (O_APPEND /*O_ASYNC, O_DIRECT, O_NOATIME,*/ | O_NONBLOCK);
            flags &= flags_mask;
            VFS::file_descriptors[sys_fd]->flags &= ~flags_mask;
            VFS::file_descriptors[sys_fd]->flags |= flags;
            return 0;
        }
        case F_SETFD:
        {
            int flags = va_arg(arg, int);
            file_descriptors[fd]->clo_exec = flags & FD_CLOEXEC;
            return 0;
        }
        default:
            return -EINVAL; // op not supported
    }
}

int Process::dup(int oldfd)
{
    int sys_fd = proc_to_sys_fd(oldfd);
    if (sys_fd == -1)
        return -EBADF;

    int newfd = get_free_fd();
    if (newfd == -1)
        return -EMFILE;

    file_descriptors[newfd] = new file_descriptor{newfd, sys_fd, false};
    return newfd;
}

int Process::dup2(int oldfd, int newfd)
{
    int sys_fd = proc_to_sys_fd(oldfd);
    if (sys_fd == -1)
        return -EBADF;

    if (oldfd == newfd)
        return newfd;

    if (file_descriptors[newfd] != nullptr)
        close(newfd);

    file_descriptors[newfd] = new file_descriptor{newfd, sys_fd, false};
    while (lowest_free_fd < MAX_FD_PER_PROCESS && file_descriptors[lowest_free_fd] != nullptr) lowest_free_fd++;

    return newfd;
}

int Process::pipe(int pipefd[2])
{
    int rfd = get_free_fd();
    if (rfd == -1)
        return -EMFILE;
    int wfd = get_free_fd();
    if (wfd == -1)
    {
        release_fd(rfd);
        return -EMFILE;
    }

    int sys_pipefd[2];
    int status = VFS::pipe(sys_pipefd);
    if (status != 0)
    {
        release_fd(rfd);
        release_fd(wfd);
        return status;
    }

    file_descriptors[rfd] = new file_descriptor(rfd, sys_pipefd[0]);
    file_descriptors[wfd] = new file_descriptor(wfd, sys_pipefd[1]);

    pipefd[0] = rfd;
    pipefd[1] = wfd;

    return 0;
}

int Process::chdir(const char* path)
{
    const auto dir = VFS::browse_to(path);

    if (!dir)
        return -ENOTDIR;

    delete[] work_dir;
    work_dir = dir->get_absolute_path();

    return 0;
}

int Process::isatty(int fd) const
{
    if (fd >= MAX_FD_PER_PROCESS)
        return -EBADF;
    const auto& proc_fd = file_descriptors[fd];
    if (proc_fd == nullptr)
        return -EBADF;

    return VFS::isatty(proc_fd->sys_fd);
}

int Process::sigaction(int signum, const struct sigaction* act, struct sigaction* old_act)
{
    if (signum > HIGHEST_SIGNAL)
    {
        if (signum == SIGCANCEL)
        {
            if (old_act)
                irrecoverable_error("%s: SIGCANCEL with non-null old_act is not supported", __func__);
            // Silently skip the request
            // For now that signal is not supported, but this returns avoid early failures
            // An error is raised in @kill if the signal is actually used though
            return 0;
        }
        printf_warn("%s: Signal not supported (signum (%d) > HIGHEST_SIGNAL (%d))",  __PRETTY_FUNCTION__, signum, HIGHEST_SIGNAL);
        return -EINVAL;
    }

    // Ensure there are no flags
    constexpr auto supported_flags = SA_RESTORER;
    if ((act->sa_flags & ~supported_flags) != 0)
    {
        printf_warn("%s: sa_flags contains the (yet) following unsupported bits: '0x%x'", __func__, (int)(act->sa_flags & ~supported_flags));
        return -EINVAL;
    }
    // Ensure mask is empty
    const bool mask_is_zeroed = *(char*)&act->sa_mask == '\0' &&
        !memcmp(&act->sa_mask, (char*)(&act->sa_mask) + 1, sizeof(act->sa_mask) - 1);
    if (!mask_is_zeroed)
    {
        printf_warn("%s: sa_mask not empty, not supported yet", __func__);
        return -EINVAL;
    }

    // Register signal new action
    auto prev = register_signal_handler(signum, act->sa_handler);

    if ((int)prev < 0)
        return (int)prev;


    struct sigaction* sig_old_act = oldact[signum];
    if (!oldact[signum]) // If no oldact for current signal, build one
    {
        oldact[signum] = new struct sigaction();
        sig_old_act = oldact[signum];
        sig_old_act->sa_handler = prev;
        sig_old_act->sa_restorer = nullptr;
        sig_old_act->sa_flags = SA_RESTORER;
    }
    *old_act = *sig_old_act;
    *oldact[signum] = *act;

    return 0;
}

int Process::sigprogmask(int how, const sigset_t* set, sigset_t* oldset)
{
    auto context = signals_contexts.peek();
    if (!context)
        irrecoverable_error("sigprocmask: no current signal context");

    switch (how)
    {
        case SIG_SETMASK:
        {
            if (!set)
                return -EFAULT;

            context->blocked_mask = *set;
            if (oldset)
                *oldset = context->blocked_mask;
            break;
        }
        case SIG_BLOCK:
        {
            if (!set)
                return -EFAULT;
            if (oldset)
                *oldset = context->blocked_mask;
            uint i = 0;
            for (const auto& m :  set->__sig)
                context->blocked_mask.__sig[i++] |= m;
            break;
        }
        case SIG_UNBLOCK:
        {
            if (!set)
                return -EFAULT;
            if (oldset)
                *oldset = context->blocked_mask;
            uint i = 0;
            for (const auto& m :  set->__sig)
                context->blocked_mask.__sig[i++] &= ~m;
            break;
        }
        default:
        {
            return -EINVAL;
        }
    }

    return 0;
}
