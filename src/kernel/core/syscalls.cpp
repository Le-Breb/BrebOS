#include "syscalls.h"
#include "interrupts.h"
#include "../processes/scheduler.h"
#include "system.h"
#include "PIC.h"
#include "../file_management/VFS.h"
#include "fb.h"
#include "GDT.h"
#include "stdarg.h"
#include "../file_management/superblock.h"
#include "../network/DNS.h"
#include "../network/HTTP.h"
#include <errno.h>

void Syscall::start_process(Process* p)
{
    // Load child process and set it ready
    auto cpu = &p->cpu_state;
    cpu->eax = Scheduler::exec((char*)cpu->edi, Scheduler::get_running_process_pid(), (int)cpu->esi,
                    (const char**)cpu->edx);

    // Register child on success
    if (cpu->eax != (uint)-1)
        p->children.add(cpu->eax);
}

void Syscall::get_pid()
{
    Process* running_process = Scheduler::get_running_process();

    running_process->cpu_state.eax = running_process->get_pid(); // Return PID
}

[[noreturn]] void Syscall::terminate_process(Process* p, int ret_val)
{
    p->terminate(ret_val);

    // We definitely do not want to continue as next step is to resume the process we just terminated !
    // Instead, we manually raise a timer interrupt which will schedule another process
    // (The terminated one has its terminated flag set, so it won't be selected by the scheduler)
    TRIGGER_TIMER_INTERRUPT

    // We will never resume the code here
    __builtin_unreachable();
}

void Syscall::malloc(Process* p)
{
    p->cpu_state.eax = (uint)p->malloc(p->cpu_state.edi);
}

void Syscall::calloc(Process* p)
{
    p->cpu_state.eax = (uint)p->calloc(p->cpu_state.edi, p->cpu_state.esi);
}

void Syscall::free(Process* p)
{
    p->free((void*)p->cpu_state.edi);
}

void Syscall::realloc(Process* p)
{
    p->cpu_state.eax = (uint)p->realloc((void*)p->cpu_state.edi, (size_t)p->cpu_state.esi);
}

void Syscall::dynlk(const cpu_state_t* cpu_state)
{
    // Get calling process and verify its identity is as expected
    Process* p = Scheduler::get_running_process();

    // Find the ELF from which the call is from
    int dep_id = 0;
    elf_dependence* dep = nullptr;
    for (dep_id = 0; dep_id < p->elf_dep_list->size(); dep_id++)
    {
        dep = p->elf_dep_list->get(dep_id);
        if ((uint)dep->elf == cpu_state->esi)
            break;
    }
    if (dep == nullptr)
    {
        printf_error("dynlk met unknown ELF");
        terminate_process(p, INIT_ERR_RET_VAL);
    }
    ELF* elf = dep->elf;

    // Get relocation table
    Elf32_Rel* reloc_table = elf->plt_relocs;
    if (reloc_table == nullptr)
    {
        printf_error("no reloc table");
        terminate_process(p, INIT_ERR_RET_VAL);
    }

    // Check required symbol's relocation
    auto* rel = (Elf32_Rel*)((uint)reloc_table + cpu_state->edi);
    uint symbol_id = ELF32_R_SYM(rel->r_info);
    uint type = ELF32_R_TYPE(rel->r_info);
    if (type != R_386_JMP_SLOT)
    {
        printf_error("Unhandled relocation type: %d", type);
        terminate_process(p, INIT_ERR_RET_VAL);
    }

    // Get dynsym and its string table
    Elf32_Shdr* dynsym_hdr = elf->dynsym_hdr;
    if (dynsym_hdr == nullptr)
    {
        printf_error("Where tf is dynsym ?");
        terminate_process(p, INIT_ERR_RET_VAL);
    }

    // Check symbol index makes sense
    uint dynsym_num_entries = dynsym_hdr->sh_size / dynsym_hdr->sh_entsize;
    if (symbol_id + 1 > dynsym_num_entries)
        printf_error("Required symbol has index %d while symtab only contains %d entries", symbol_id,
                     dynsym_num_entries);

    // Get symbol name
    Elf32_Sym* s = &elf->dynsym[symbol_id];
    const char* symbol_name = &elf->dynsym_strtab[s->st_name];

    // Find symbol
    void* symbol_addr = (void*)p->get_symbol_runtime_address_at_runtime(dep_id, symbol_name);
    if (symbol_addr == nullptr)
    {
        printf_error("Symbol %s not found", symbol_name);
        return;
    }

    // Compute symbol's GOT entry address
    uint got_entry_addr = rel->r_offset + dep->runtime_load_address;

    *(void**)got_entry_addr = symbol_addr; // Write symbol address to GOT
    Scheduler::get_running_process()->cpu_state.eax = (uint)symbol_addr; // Return symbol address to userland dynlk
}

__attribute__((no_instrument_function)) // May not return, which would mess up profiling data
void Syscall::get_key()
{
    Scheduler::get_running_process()->set_flag(P_WAITING_KEY);

    TRIGGER_TIMER_INTERRUPT
}

[[noreturn]]
void Syscall::dispatcher(const cpu_state_t* cpu_state, const stack_state_t* stack_state)
{
    Process* p = Scheduler::get_running_process();

    // Update PCB
    p->cpu_state = *cpu_state;
    p->stack_state = *stack_state;

    switch (cpu_state->eax)
    {
        case 1:
            terminate_process(p, (int)cpu_state->edi);
        case 2:
            FB::write((char*)cpu_state->esi);
            break;
        case 3:
            PIC::disable_preemptive_scheduling(); // Is it necessary ? Isn't timer interrupt disabled when this runs ?
            start_process(p);
            PIC::enable_preemptive_scheduling();
            break;
        case 4:
            get_key();
            break;
        case 5:
            get_pid();
            break;
        case 6:
            System::shutdown();
        case 7:
            dynlk(&p->cpu_state);
            break;
        case 8:
            malloc(p);
            break;
        case 9:
            free(p);
            break;
        case 10:
            mkdir(&p->cpu_state);
            break;
        case 11:
            touch(&p->cpu_state);
            break;
        case 12:
            ls(&p->cpu_state);
            break;
        case 13:
            FB::clear_screen();
            break;
        case 14:
            p->cpu_state.eax = wait_pid(p);
            break;
        case 15:
            wget(&p->cpu_state);
            break;
        case 16:
            p->cpu_state.eax = (uint)stat(p);
            break;
        case 17:
            calloc(p);
            break;
        case 18:
            realloc(p);
            break;
        case 19:
            p->cpu_state.eax = tcbset(p);
            break;
        case 20:
            p->cpu_state.eax = Scheduler::execve(p, (const char*)p->cpu_state.edi, (int)p->cpu_state.esi,
                                                 (const char**)p->cpu_state.edx, false);
            break;
        case 21:
            feh(p);
            break;
        case 22:
            FB::lock_flushing();
            break;
        case 23:
            FB::unlock_flushing();
            break;
        case 24:
            get_screen_dimensions(p);
            break;
        case 25:
            load_file(p);
            break;
        case 26:
            p->cpu_state.eax = write(p);
            break;
        case 28:
            p->cpu_state.eax = p->fork();
            break;
        case 29:
        {
            SharedPointer<Dentry> file = VFS::browse_to((char*)p->cpu_state.edi);
            p->cpu_state.eax = file ? file->inode->size : (uint)-1;
            break;
        }
        case 30:
            p->cpu_state.eax = open(p);
            break;
        case 31:
            p->cpu_state.eax = read(p);
            break;
        case 32:
            p->cpu_state.eax = close(p);
            break;
        case 33:
            p->cpu_state.eax = lseek(p);
            break;
        case 34:
            p->cpu_state.eax = fstat(p);
            break;
        case 35:
            p->cpu_state.eax = kill(p);
            break;
        case 36:
            p->cpu_state.eax = (uint)signal(p);
            break;
        case 37:
            signal_return(p);
            break;
        case 38:
            p->cpu_state.eax = fcntl(p);
            break;
        case 39:
            p->cpu_state.eax = dup2(p);
            break;
        case 40:
            p->cpu_state.eax = dup(p);
            break;
        case 41:
            p->cpu_state.eax = pipe(p);
            break;
        case 42: // Execvp
            p->cpu_state.eax = Scheduler::execve(p, (const char*)p->cpu_state.edi, (int)p->cpu_state.esi,
                                                 (const char**)p->cpu_state.edx, true);
            break;
        case 43:
            getcwd(p);
            break;
        case 44:
            p->cpu_state.eax= chdir(p);
            break;
        case 45:
            p->cpu_state.eax = sigprocmask(p);;
            break;
        case 46:
            p->cpu_state.eax = isatty(p);
            break;
        case 47:
            p->cpu_state.eax = sigaction(p);
            break;
        default:
            printf_error("Received unknown syscall id: 0x%x", cpu_state->eax);
            break;
    }

    Scheduler::resume_user_process(p);
}

void Syscall::wget(const cpu_state_t* cpu_state)
{
    const char* uri = (const char*)cpu_state->edi;
    const char* hostname = (const char*)cpu_state->esi;
    const auto port = (uint16_t)cpu_state->edx;
    auto http = new HTTP{hostname, port};
    http->send_get(uri);
}

void Syscall::mkdir(cpu_state_t* cpu_state)
{
    const char* path = (const char*)cpu_state->edi;

    cpu_state->eax = (uint)(VFS::mkdir(path) ? 1 : 0);
}

void Syscall::touch(cpu_state_t* cpu_state)
{
    const char* path = (const char*)cpu_state->edi;

    cpu_state->eax = (uint)(VFS::touch(path) ? 1 : 0);
}

void Syscall::ls(cpu_state_t* cpu_state)
{
    const char* path = (const char*)cpu_state->edi;

    cpu_state->eax = (uint)VFS::ls(path);
}

int Syscall::lseek(const Process* p)
{
    int fd = (int)p->cpu_state.edi;
    int offset = (int)p->cpu_state.esi;
    int whence = (int)p->cpu_state.edx;

    return p->lseek(fd, offset, whence);
}

__attribute__((no_instrument_function))
void Syscall::feh(Process* p)
{
    // Gather args
    auto rgb = (unsigned char*)p->cpu_state.edi;
    uint x = p->cpu_state.esi;
    uint y = p->cpu_state.edx;

    // Display image and set process asleep
    FB::draw_rgb(rgb, x, y);
    Scheduler::set_process_asleep(p, 4000); // Arbitrary sleep duration

    // Trigger timer interrupt to schedule another process and actually sleep
    TRIGGER_TIMER_INTERRUPT
}

extern "C" const uint32_t SCREEN_WIDTH;
extern "C" const uint32_t SCREEN_HEIGHT;
void Syscall::get_screen_dimensions(Process* p)
{
    p->cpu_state.eax = SCREEN_WIDTH;
    p->cpu_state.edi = SCREEN_HEIGHT;
}

void Syscall::load_file(Process* p)
{
    // Get path
    auto path = (const char*)p->cpu_state.edi;

    // Get file
    SharedPointer<Dentry> dentry = VFS::browse_to(path);
    if (!dentry)
    {
        p->cpu_state.eax = 0;
        return;
    }

    uint size = dentry->inode->size;

    // Load file in user space
    auto f = VFS::load_file(dentry, 0, size);
    if (!f)
    {
        p->cpu_state.eax = 0;
        return;
    }
    memcpy((void*)p->cpu_state.esi, f, size);

    // Write return values
    p->cpu_state.eax = 1;
    p->cpu_state.edi = dentry->inode->size;

    delete[] (char*)f;
}

int Syscall::write(Process* p)
{
    int fd = (int)p->cpu_state.edi;
    void* buf = (void*)p->cpu_state.esi;
    uint count = p->cpu_state.edx;

    return p->write(fd, count, buf);
}

int Syscall::open(Process* p)
{
    const char* pathname = (const char*)p->cpu_state.edi;
    int flags = (int)p->cpu_state.esi;
    mode_t mode = (int)p->cpu_state.edx;

    return p->open(pathname, flags, mode);
}

int Syscall::read(Process* p)
{
    int fd = (int)p->cpu_state.edi;
    void* buf = (void*)p->cpu_state.esi;
    uint len = p->cpu_state.edx;

    if (buf == nullptr)
        return -EFAULT;

    return p->read(fd, buf, len);
}

int Syscall::close(Process* p)
{
    int fd = (int)p->cpu_state.edi;

    return p->close(fd);
}

int Syscall::stat(const Process* p)
{
    const char* pathname = (const char*)p->cpu_state.edi;
    auto statbuf = (struct stat*)p->cpu_state.esi;

    return p->stat(pathname, statbuf);
}

int Syscall::fstat(const Process* p)
{
    int proc_fd = (int)p->cpu_state.edi;
    auto statbuf = (struct stat*)p->cpu_state.esi;

    return p->fstat(proc_fd, statbuf);
}

int Syscall::kill(const Process* p)
{
    int pid = (int)p->cpu_state.edi;
    int signal = (int)p->cpu_state.esi;

    Process* proc = Scheduler::get_process(pid);
    if (!proc)
        return -ESRCH; // No such process

    return proc->kill(signal);
}

__sighandler Syscall::signal(Process* p)
{
    int signal = (int)p->cpu_state.edi;
    auto handler = (__sighandler)p->cpu_state.esi;

    return p->register_signal_handler(signal, handler);
}

void Syscall::signal_return(Process* p)
{
    p->signal_context_restore();
}

int Syscall::fcntl(Process* p)
{
    int fd = (int)p->cpu_state.edi;
    int op = (int)p->cpu_state.esi;
    auto arg = (va_list)p->cpu_state.edx;

    return p->fcntl(fd, op, arg);
}

int Syscall::dup(Process* p)
{
    int fd = p->cpu_state.edi;

    return p->dup(fd);
}

int Syscall::dup2(Process* p)
{
    int oldfd = p->cpu_state.edi;
    int newfd = p->cpu_state.esi;

    return p->dup2(oldfd, newfd);
}

int Syscall::pipe(Process* p)
{
    int* pipefd = (int*)p->cpu_state.edi;

    return p->pipe(pipefd);
}

void Syscall::getcwd(Process* p)
{
#define getcwd_ret_err(err) { p->cpu_state.eax = 0; p->cpu_state.edi = err; return; }

    auto buf = (char*)p->cpu_state.edi;
    auto size = (size_t)p->cpu_state.esi;

    if (size == 0 || !buf)
        getcwd_ret_err(EINVAL)
    if (strlen(p->get_work_dir()) > size)
        getcwd_ret_err(ERANGE)

    strcpy(buf, p->get_work_dir());
    p->cpu_state.eax = (int)buf;
}

int Syscall::chdir(Process* p)
{
    auto path = (const char*)p->cpu_state.edi;

    return p->chdir(path);
}

uint Syscall::tcbset(Process* p)
{
    void* addr = (void*)p->cpu_state.edi;
    p->tls_base = addr;
    GDT::set_tls(addr);

    return TLS_ENTRY;
}

int Syscall::isatty(const Process* process)
{
    int fd = process->cpu_state.edi;
    return process->isatty(fd);
}

int Syscall::sigaction(Process* p)
{
    int signum = p->cpu_state.edi;
    const auto* act = (const struct sigaction*)p->cpu_state.esi;
    auto* old_act = (struct sigaction*)p->cpu_state.edx;

    return p->sigaction(signum, act, old_act);
}

int Syscall::sigprocmask(Process* p)
{
    int how = p->cpu_state.edi;
    const auto* set = (const sigset_t*)p->cpu_state.esi;
    auto* oldset = (sigset_t*)p->cpu_state.edx;

    return p->sigprogmask(how, set, oldset);
}

__attribute__((no_instrument_function)) // May not return, which would mess up profiling data
int Syscall::wait_pid(Process* p)
{
    int wait = Scheduler::register_process_wait(p->get_pid(), (int)p->cpu_state.edi);

    // Direct return, either error or child already terminated
    if (wait != 0)
    {
        if (wait > 0) // set wstatus. See newlib's wait.h
            *(int*)p->cpu_state.esi = wait << 8;
        return wait;
    }

    // Wait
    TRIGGER_TIMER_INTERRUPT

    return (int)p->cpu_state.eax; // Return value is written here by Scheduler
}
