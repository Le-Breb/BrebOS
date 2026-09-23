#include "syscalls.h"
#include "syscall_numbers.h"
#include "interrupts.h"
#include "../processes/scheduler.h"
#include "system.h"
#include "PIC.h"
#include "../file_management/VFS.h"
#include "fb.h"
#include "GDT.h"
#include "stdarg.h"
#include "../file_management/superblock.h"
#include "../network/HTTP.h"
#include <errno.h>
#include <bits/wint_t.h>

#include "PIT.h"
#include "../misc/GDB.h"
#include "abi-bits/fcntl.h"
#include "abi-bits/wait.h"

Syscall::SyscallResult Syscall::get_pid(Process* p)
{
    p->cpu_state.eax = p->get_pid(); // Return PID

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::terminate_process(Process* p, int ret_val)
{
    p->terminate_with_value(ret_val);

    return SyscallResult::ScheduleExit;
}

Syscall::SyscallResult Syscall::malloc(Process* p)
{
    p->cpu_state.eax = (uint)p->malloc(p->cpu_state.ebx);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::calloc(Process* p)
{
    p->cpu_state.eax = (uint)p->calloc(p->cpu_state.ebx, p->cpu_state.ecx);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::free(Process* p)
{
    p->free((void*)p->cpu_state.ebx);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::realloc(Process* p)
{
    p->cpu_state.eax = (uint)p->realloc((void*)p->cpu_state.ebx, (size_t)p->cpu_state.ecx);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::get_key()
{
    Scheduler::get_running_process()->set_flag(P_WAITING_KEY);

    return SyscallResult::Schedule;
}

[[noreturn]]
void Syscall::dispatcher(const cpu_state_t* cpu_state, const stack_state_t* stack_state)
{
    // As syscalls are not concurrency-proof (yet ?), preemption is prevented by setting this lock.
    // Interrupt_timer still runs, but won't call schedule and simply resume this syscall
    Scheduler::preemption_lock = true;

    Process* p = Scheduler::get_running_process();

    // Update PCB
    p->cpu_state = *cpu_state;
    p->stack_state = *stack_state;

    const SyscallResult result = [&]()
    {
        switch (cpu_state->eax)
        {
            case SyscallNumber::TERMINATE_PROCESS:
                return terminate_process(p, (int)cpu_state->ebx);
            case SyscallNumber::PRINTF:
                FB::write((char*)cpu_state->ebx);
                return SyscallResult::ReturnToUser;
            case SyscallNumber::GDB_NOTIFY_ELF:
            {
                if (const bool load = (bool)p->cpu_state.ecx; load)
                    GDB::get_instance()->load_elf((const char*)cpu_state->ebx);
                else
                    GDB::get_instance()->unload_elf((const char*)cpu_state->ebx);
                return SyscallResult::ReturnToUser;
            }
            case SyscallNumber::GET_KEY:
                get_key();
                return SyscallResult::Schedule;
            case SyscallNumber::GET_PID:
                get_pid(p);
                return SyscallResult::ReturnToUser;
            case SyscallNumber::SHUTDOWN:
                System::shutdown();
            case SyscallNumber::SLEEP:
                return sleep(p);
            case SyscallNumber::MALLOC:
                return malloc(p);
            case SyscallNumber::FREE:
                return free(p);
            case SyscallNumber::MKDIR:
                return mkdir(&p->cpu_state);
            case SyscallNumber::TOUCH:
                return touch(&p->cpu_state);
            case SyscallNumber::CLEAR_SCREEN:
                FB::clear_screen();
                return SyscallResult::ReturnToUser;
            case SyscallNumber::WAIT_PID:
                return wait_pid(p);
            case SyscallNumber::WGET:
                wget(&p->cpu_state);
                return SyscallResult::ReturnToUser;
            case SyscallNumber::STAT:
                return stat(p);
            case SyscallNumber::CALLOC:
                return calloc(p);
            case SyscallNumber::REALLOC:
                return realloc(p);
            case SyscallNumber::TCBSET:
                return tcbset(p);
            case SyscallNumber::EXECVE:
                return execve(p);
            case SyscallNumber::FEH:
                return feh(p);
            case SyscallNumber::LOCK_FLUSHING:
                FB::lock_flushing();
                return SyscallResult::ReturnToUser;
            case SyscallNumber::UNLOCK_FLUSHING:
                FB::unlock_flushing();
                return SyscallResult::ReturnToUser;
            case SyscallNumber::GET_SCREEN_DIMENSIONS:
                get_screen_dimensions(p);
                return SyscallResult::ReturnToUser;
            case SyscallNumber::LOAD_FILE:
                load_file(p);
                return SyscallResult::ReturnToUser;
            case SyscallNumber::WRITE:
                return write(p);
            case SyscallNumber::OPENDIR:
                return opendir(p);
            case SyscallNumber::FORK:
                p->cpu_state.eax = p->fork();
                return SyscallResult::ReturnToUser;
            case SyscallNumber::GET_FILE_SIZE:
            {
                const SharedPointer<Dentry> file = VFS::browse_to((char*)p->cpu_state.ebx);
                p->cpu_state.eax = file ? file->inode->size : (uint)-1;
                return SyscallResult::ReturnToUser;
            }
            case SyscallNumber::OPEN:
                return open(p);
            case SyscallNumber::READ:
                return read(p);
            case SyscallNumber::CLOSE:
                return close(p);
            case SyscallNumber::LSEEK:
                return lseek(p);
            case SyscallNumber::FSTAT:
                return fstat(p);
            case SyscallNumber::KILL:
                return kill(p);
            case SyscallNumber::SIGNAL:
                return signal(p);
            case SyscallNumber::SIGNAL_RETURN:
                signal_return(p);
                return SyscallResult::ReturnToUser;
            case SyscallNumber::FCNTL:
                return fcntl(p);
            case SyscallNumber::DUP2:
                return dup2(p);
            case SyscallNumber::DUP:
                return dup(p);
            case SyscallNumber::PIPE:
                return pipe(p);
            case SyscallNumber::GETCWD:
                getcwd(p);
                return SyscallResult::ReturnToUser;
            case SyscallNumber::CHDIR:
                return chdir(p);
            case SyscallNumber::SIGPROCMASK:
                return sigprocmask(p);;
            case SyscallNumber::ISATTY:
                return isatty(p);
            case SyscallNumber::SIGACTION:
                return sigaction(p);
            case SyscallNumber::MMAP:
                return mmap(p);;
            case SyscallNumber::MPROTECT:
                return mprotect(p);
            case SyscallNumber::GDB_UNLOAD_ELF:
                GDB::get_instance()->unload_elf((const char*)p->cpu_state.ebx);
                return SyscallResult::ReturnToUser;
            case SyscallNumber::GETDENTS:
                return getdents(p);
            case SyscallNumber::DEBUG_PRINT:
                FB::flush();
                printf_info("%d | 0x%x", p->cpu_state.ebx, p->cpu_state.ebx);
                return SyscallResult::ReturnToUser;
            default:
                printf_error("Received unknown syscall id: 0x%x", cpu_state->eax);
                return SyscallResult::ReturnToUser;
        }
    }();

    Scheduler::preemption_lock = false;

    switch (result)
    {
        case SyscallResult::Schedule:
            TRIGGER_TIMER_INTERRUPT
            Scheduler::resume_user_process(p);
        case SyscallResult::ReturnToUser:
            Scheduler::resume_user_process(p);
        case SyscallResult::ScheduleExit:
            TRIGGER_TIMER_INTERRUPT
    }
    irrecoverable_error("unreachable code reached");
}

Syscall::SyscallResult Syscall::wget(const cpu_state_t* cpu_state)
{
    const char* uri = (const char*)cpu_state->ebx;
    const char* hostname = (const char*)cpu_state->ecx;
    const auto port = (uint16_t)cpu_state->edx;
    auto http = new HTTP{hostname, port};
    http->send_get(uri);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::mkdir(cpu_state_t* cpu_state)
{
    const char* path = (const char*)cpu_state->ebx;
    cpu_state->eax = (uint)(VFS::mkdir(path) ? 1 : 0);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::touch(cpu_state_t* cpu_state)
{
    const char* path = (const char*)cpu_state->ebx;
    cpu_state->eax = (uint)(VFS::touch(path) ? 1 : 0);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::lseek(Process* p)
{
    int fd = (int)p->cpu_state.ebx;
    int offset = (int)p->cpu_state.ecx;
    int whence = (int)p->cpu_state.edx;

    p->cpu_state.eax = p->lseek(fd, offset, whence);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::feh(Process* p)
{
    // Gather args
    auto rgb = (unsigned char*)p->cpu_state.ebx;
    uint x = p->cpu_state.ecx;
    uint y = p->cpu_state.edx;

    // Display image and set process asleep
    FB::draw_rgb(rgb, x, y);
    Scheduler::set_process_asleep(p, 4000); // Arbitrary sleep duration

    return SyscallResult::Schedule;
}

extern "C" const uint32_t SCREEN_WIDTH;
extern "C" const uint32_t SCREEN_HEIGHT;
Syscall::SyscallResult Syscall::get_screen_dimensions(Process* p)
{
    p->cpu_state.eax = SCREEN_WIDTH;
    p->cpu_state.edi = SCREEN_HEIGHT;

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::load_file(Process* p)
{
    // Get path
    auto path = (const char*)p->cpu_state.ebx;

    // Get file
    SharedPointer<Dentry> dentry = VFS::browse_to(path);
    if (!dentry)
    {
        p->cpu_state.eax = 0;
        return SyscallResult::ReturnToUser;
    }

    uint size = dentry->inode->size;

    // Load file in user space
    auto f = VFS::load_file(dentry, 0, size);
    if (!f)
    {
        p->cpu_state.eax = 0;
        return SyscallResult::ReturnToUser;
    }
    memcpy((void*)p->cpu_state.ecx, f, size);

    // Write return values
    p->cpu_state.eax = 1;
    p->cpu_state.edi = dentry->inode->size;

    delete[] (char*)f;

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::write(Process* p)
{
    int fd = (int)p->cpu_state.ebx;
    void* buf = (void*)p->cpu_state.ecx;
    uint count = p->cpu_state.edx;

    p->cpu_state.eax = p->write(fd, count, buf);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::open(Process* p)
{
    const char* pathname = (const char*)p->cpu_state.ebx;
    int flags = (int)p->cpu_state.ecx;
    mode_t mode = (int)p->cpu_state.edx;

    p->cpu_state.eax = p->open(pathname, flags, mode);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::read(Process* p)
{
    int fd = (int)p->cpu_state.ebx;
    void* buf = (void*)p->cpu_state.ecx;
    uint len = p->cpu_state.edx;

    p->cpu_state.eax = buf == nullptr ? -EFAULT : p->read(fd, buf, len);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::close(Process* p)
{
    int fd = (int)p->cpu_state.ebx;

    p->cpu_state.eax = p->close(fd);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::stat(Process* p)
{
    const char* pathname = (const char*)p->cpu_state.ebx;
    auto statbuf = (struct stat*)p->cpu_state.ecx;

    p->cpu_state.eax = (uint)p->stat(pathname, statbuf);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::fstat(Process* p)
{
    int proc_fd = (int)p->cpu_state.ebx;
    auto statbuf = (struct stat*)p->cpu_state.ecx;

    p->cpu_state.eax = p->fstat(proc_fd, statbuf);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::kill(Process* p)
{
    int pid = (int)p->cpu_state.ebx;
    int signal = (int)p->cpu_state.ecx;

    Process* proc = Scheduler::get_process(pid);
    p->cpu_state.eax = !proc ? -ESRCH : proc->kill(signal);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::signal(Process* p)
{
    int signal = (int)p->cpu_state.ebx;
    auto handler = (__sighandler)p->cpu_state.ecx;

    p->cpu_state.eax = (uint)p->register_signal_handler(signal, handler);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::signal_return(Process* p)
{
    p->signal_context_restore();
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::fcntl(Process* p)
{
    int fd = (int)p->cpu_state.ebx;
    int op = (int)p->cpu_state.ecx;
    auto arg = (va_list)p->cpu_state.edx;

    p->cpu_state.eax = p->fcntl(fd, op, arg);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::dup(Process* p)
{
    int fd = p->cpu_state.ebx;

    p->cpu_state.eax = p->dup(fd);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::dup2(Process* p)
{
    int oldfd = p->cpu_state.ebx;
    int newfd = p->cpu_state.ecx;

    p->cpu_state.eax = p->dup2(oldfd, newfd);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::pipe(Process* p)
{
    int* pipefd = (int*)p->cpu_state.ebx;

    p->cpu_state.eax = p->pipe(pipefd);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::getcwd(Process* p)
{
#define getcwd_ret_err(err) { p->cpu_state.eax = 0; p->cpu_state.edi = err; return SyscallResult::ReturnToUser;; }

    auto buf = (char*)p->cpu_state.ebx;
    auto size = (size_t)p->cpu_state.ecx;

    if (size == 0 || !buf)
        getcwd_ret_err(EINVAL)
    if (strlen(p->get_work_dir()) > size)
        getcwd_ret_err(ERANGE)

    strcpy(buf, p->get_work_dir());
    p->cpu_state.eax = (int)buf;
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::chdir(Process* p)
{
    const auto path = (const char*)p->cpu_state.ebx;

    p->cpu_state.eax = p->chdir(path);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::tcbset(Process* p)
{
    void* addr = (void*)p->cpu_state.ebx;
    p->tls_base = addr;
    GDT::set_tls(addr);

    p->cpu_state.eax = TLS_ENTRY;
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::isatty(Process* process)
{
    int fd = process->cpu_state.ebx;
    process->cpu_state.eax = process->isatty(fd);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::sigaction(Process* p)
{
    int signum = p->cpu_state.ebx;
    const auto* act = (const struct sigaction*)p->cpu_state.ecx;
    auto* old_act = (struct sigaction*)p->cpu_state.edx;

    p->cpu_state.eax = p->sigaction(signum, act, old_act);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::sigprocmask(Process* p)
{
    int how = p->cpu_state.ebx;
    const auto* set = (const sigset_t*)p->cpu_state.ecx;
    auto* oldset = (sigset_t*)p->cpu_state.edx;

    p->cpu_state.eax = p->sigprogmask(how, set, oldset);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::mmap(Process* p)
{
	void* hint = (void*)p->cpu_state.ebx;
	size_t size = p->cpu_state.ecx;
	int prot = (int)p->cpu_state.edx;
	int flags = (int)p->cpu_state.esi;
	int fd = (int)p->cpu_state.edi;
	off_t offset  = p->cpu_state.ebp;

	int err = 0;
	const void* window = Memory::mmap(hint, size, prot, flags, fd, offset, err, p, false, true);

	p->cpu_state.eax = window ? (int)window : -err;
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::mprotect(Process* p)
{
	void* addr = (void*)p->cpu_state.ebx;
	size_t len = p->cpu_state.ecx;
	int prot = (int)p->cpu_state.edx;

	p->cpu_state.eax = Memory::mprotect(addr, len, prot, p);
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::execve(Process* p)
{
    const auto path = (char*)p->cpu_state.ebx;
    const auto argc = (int)p->cpu_state.ecx;
    const auto argv = (const char**)p->cpu_state.edx;
    const auto envp = (const char**)p->cpu_state.esi;

    if (Scheduler::execve(p, path, argc, argv, envp))
        return SyscallResult::ScheduleExit;

    p->cpu_state.eax = -1;
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::sleep(Process* p)
{
    if (const long ns = *(long*)p->cpu_state.ecx)
    {
        printf_warn("%s called 'sleep' with nano seconds, only seconds are supported for now", p->bin_path);
        if (p->kill(SIGQUIT) < 0)
            irrecoverable_error("couldn't send SIGQUIT to process %d (%s)", p->get_pid(), p->bin_path);
        return SyscallResult::ReturnToUser;
    }

    const time_t s = *(time_t*)p->cpu_state.ebx;
    PIT::sleep(s * 1000);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::opendir(Process* process)
{
    const char* path = (const char*)process->cpu_state.ebx;

    const int tmp_fd = process->open(path, O_RDONLY, 0777);
    if (tmp_fd < 0)
    {
        process->cpu_state.eax = tmp_fd;
        return SyscallResult::ReturnToUser;
    }

    const int sys_fd = process->proc_to_sys_fd(tmp_fd);
    if (sys_fd == -1)
    {
        printf_warn("%s: open succeeded but proc_to_sys_fd failed", __func__);
        process->kill(SIGTERM);
        return SyscallResult::ReturnToUser;
    }
    const auto sys_fi = VFS::file_descriptors[sys_fd];
    if (!sys_fi)
    {
        printf_warn("%s: No FileInterface matching sys_fd was found", __func__);
        process->kill(SIGTERM);
        return SyscallResult::ReturnToUser;
    }
    if (sys_fi->type != FileInterface::File)
    {
        printf_warn("%s: FileInterface is not of type File", __func__);
        process->kill(SIGTERM);
        return SyscallResult::ReturnToUser;
    }
    if (((File*)sys_fi)->dentry->inode->type == Inode::Dir)
    {
        process->cpu_state.eax = tmp_fd;
        return SyscallResult::ReturnToUser;
    }

    process->cpu_state.eax = -ENOTDIR;
    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::getdents(Process* p)
{
    const int handle = (int)p->cpu_state.ebx;
    void *buffer = (void*)p->cpu_state.ecx;
    const size_t max_size = (size_t)p->cpu_state.edx;
    size_t *bytes_read = (size_t*)p->cpu_state.esi;

    p->cpu_state.eax = p->getdents(handle, buffer, max_size, bytes_read);

    return SyscallResult::ReturnToUser;
}

Syscall::SyscallResult Syscall::wait_pid(Process* p)
{
    const int waited_for_process = (int)p->cpu_state.ebx;
    const int flags = (int)p->cpu_state.edx;

    if (constexpr int supported_flags = WNOHANG; flags & ~supported_flags)
    {
        printf_warn("waitpid called with the following unsupported flags: 0x%x", flags & ~supported_flags);
        p->cpu_state.eax = -EINVAL;
        return SyscallResult::ReturnToUser;
    }

    bool return_now;
    const int wait = Scheduler::register_process_wait(p->get_pid(), waited_for_process, flags & WNOHANG, return_now);

    // Direct return, either error or child already terminated
    if (return_now)
    {
        if (wait > 0)
        {
            Process* waited_for_proc = Scheduler::get_process(wait);

            int* wstatus = (int*)p->cpu_state.ecx;
            *wstatus = waited_for_proc->get_ret_status();

            if (waited_for_proc->is_zombie())
                Scheduler::free_process(*waited_for_proc);
        }
        p->cpu_state.eax = wait;
        return SyscallResult::ReturnToUser;
    }

    // Wait
    // p->cpu_state.eax; // Return value is written here by Scheduler
    return SyscallResult::Schedule;
}
