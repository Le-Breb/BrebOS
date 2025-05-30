#include "syscalls.h"
#include "interrupts.h"
#include "../processes/scheduler.h"
#include <kstdio.h>
#include "system.h"
#include "PIC.h"
#include "../file_management/VFS.h"
#include "fb.h"
#include "../network/DNS.h"
#include "../network/HTTP.h"

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
    Elf32_Sym* s = &elf->symbols[symbol_id];
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
            wait_pid(p);
            break;
        case 15:
            wget(&p->cpu_state);
            break;
        case 17:
            calloc(p);
            break;
        case 18:
            realloc(p);
            break;
        case 19:
            getenv(p);
            break;
        case 20:
            p->cpu_state.eax = Scheduler::execve(p, (const char*)p->cpu_state.edi, (int)p->cpu_state.esi,
                (const char**)p->cpu_state.edx);
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
            write(p);
            break;
        case 27:
            p->cpu_state.eax = (uint)p->sbrk((int)p->cpu_state.edi);
            break;
        case 28:
            p->cpu_state.eax = p->fork();
            break;
        case 29:
        {
            Dentry* file = VFS::browse_to((char*)p->cpu_state.edi);
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
        default:
            printf_error("Received unknown syscall id: 0x%x", cpu_state->eax);
            break;
    }

    // Todo: Check if process ESP should be reset to process kernel stack top here
    Interrupts::resume_user_process_asm(&p->cpu_state, &p->stack_state);
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

void Syscall::getenv(Process* p)
{
    char* env = Process::get_env((char*)p->cpu_state.edi);
    if (env)
    {
        auto len = strlen(env);
        auto user_env = (char*)p->malloc(len + 1);
        if (!user_env)
        {
            p->cpu_state.eax = 0;
            return;
        }
        strcpy(user_env, env);
        p->cpu_state.eax = (uint)user_env;
    }
    else
        p->cpu_state.eax = 0;
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
    Dentry* dentry = VFS::browse_to(path);
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

void Syscall::write(Process* p)
{
    int file = (int)p->cpu_state.edi;
    if (file == 2)
        FB::set_fg(FB_RED);
    else if (file != 1)
    {
        p->cpu_state.eax = -1;
        return;
    }

    char* str = (char*)p->cpu_state.esi;
    uint len = p->cpu_state.edx;
    FB::write(str, len);
    if (file == 2)
        FB::set_fg(FB_WHITE);

    p->cpu_state.eax = len;
}

int Syscall::open(Process* p)
{
    const char* pathname = (const char*)p->cpu_state.edi;
    int flags = (int)p->cpu_state.esi;

    return p->open(pathname, flags);
}

int Syscall::read(Process* p)
{
    int fd = (int)p->cpu_state.edi;
    void* buf = (void*)p->cpu_state.esi;
    uint len = p->cpu_state.edx;

    return p->read(fd, buf, len);
}

int Syscall::close(Process* p)
{
    int fd = (int)p->cpu_state.edi;

    return p->close(fd);
}

__attribute__((no_instrument_function)) // May not return, which would mess up profiling data
void Syscall::wait_pid(Process* p)
{
    Scheduler::register_process_wait(p->get_pid(), (int)p->cpu_state.edi);
    TRIGGER_TIMER_INTERRUPT
}
