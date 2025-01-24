#include "syscalls.h"
#include "interrupts.h"
#include "../processes/scheduler.h"
#include "kstdio.h"
#include "system.h"
#include "PIC.h"
#include "../file_management/VFS.h"
#include "fb.h"

void Syscall::start_process(cpu_state_t* cpu_state)
{
    // Load child process and set it ready
    Scheduler::exec((char*)cpu_state->edi, Scheduler::get_running_process_pid(), cpu_state->esi,
                    (const char**)cpu_state->edx);
}

void Syscall::get_pid()
{
    Process* running_process = Scheduler::get_running_process();

    running_process->cpu_state.eax = running_process->get_pid(); // Return PID
}

[[noreturn]] void Syscall::terminate_process(Process* p)
{
    p->terminate();

    // We definitely do not want to continue as next step is to resume the process we just terminated !
    // Instead, we manually raise a timer interrupt which will schedule another process
    // (The terminated one has its terminated flag set, so it won't be selected by the scheduler)
    __asm__ volatile("int $0x20");

    // We will never resume the code here
    __builtin_unreachable();
}

void Syscall::malloc(Process* p, cpu_state_t* cpu_state)
{
    cpu_state->eax = (uint)p->allocate_dyn_memory(cpu_state->edi);
}

void Syscall::free(Process* p, cpu_state_t* cpu_state)
{
    p->free_dyn_memory((void*)cpu_state->edi);
}

void Syscall::dynlk(cpu_state_t* cpu_state)
{
    // Get calling process and verify its identity is as expected
    Process* p = Scheduler::get_running_process();
    ELF* elf;
    if ((uint)p == cpu_state->esi)
        elf = p->elf;
    else if ((uint)p->kapi_elf == cpu_state->esi)
        elf = p->kapi_elf;
    else
    {
        printf_error("dynlk met unknown ELF");
        return;
    }

    // Get relocation table
    Elf32_Rel* reloc_table = elf->relocs;
    if (reloc_table == nullptr)
    {
        printf_error("no reloc table");
        terminate_process(p);
    }

    // Check required symbol's relocation
    Elf32_Rel* rel = (Elf32_Rel*)((uint)reloc_table + cpu_state->edi);
    uint symbol = ELF32_R_SYM(rel->r_info);
    uint type = ELF32_R_TYPE(rel->r_info);
    if (type != R_386_JMP_SLOT)
    {
        printf_error("Unhandled relocation type: %d", type);
        terminate_process(p);
    }

    // Get dynsym and its string table
    Elf32_Shdr* dynsym_hdr = elf->dynsym_hdr;
    if (dynsym_hdr == nullptr)
    {
        printf_error("Where tf is dynsym ?");
        terminate_process(p);
    }

    // Check symbol index makes sense
    uint dynsym_num_entries = dynsym_hdr->sh_size / dynsym_hdr->sh_entsize;
    if (symbol + 1 > dynsym_num_entries)
        printf_error("Required symbol has index %d while symtab only contains %d entries", symbol,
                     dynsym_num_entries);

    // Get symbol name
    Elf32_Sym* s = &elf->symbols[symbol];
    const char* symbol_name = &elf->dynsym_strtab[s->st_name];

    // Get symbol in kapi
    Elf32_Sym* lib_s = p->kapi_elf->get_symbol(symbol_name);
    if (lib_s == nullptr)
    {
        printf_error("Symbol %s not found in libc", symbol_name);
        terminate_process(p);
    }

    // Compute symbol address // Todo: Make it modular, do not rely on lib to be loaded after libydnlk
    // and add support for multiple libs
    uint lib_runtime_start_addr =
        ((p->elf->get_highest_runtime_addr()+ (PAGE_SIZE - 1))& ~(PAGE_SIZE - 1)) +
            ((p->libdynlk_elf->get_highest_runtime_addr() + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1));
    void* symbol_addr = (void*)(lib_runtime_start_addr + lib_s->st_value);

    uint got_entry_addr = rel->r_offset + (((uint)p == cpu_state->esi) ? 0 : lib_runtime_start_addr);
    *((void**)got_entry_addr) = symbol_addr; // Write symbol address to GOT
    Scheduler::get_running_process()->cpu_state.eax = (uint)symbol_addr; // Return symbol address
}

void Syscall::get_key()
{
    Scheduler::get_running_process()->set_flag(P_WAITING_KEY);

    __asm__ volatile("int $0x20");
}

[[noreturn]] void Syscall::dispatcher(cpu_state_t* cpu_state, stack_state_t* stack_state)
{
    Process* p = Scheduler::get_running_process();

    // Update PCB
    p->cpu_state = *cpu_state;
    p->stack_state = *stack_state;

    switch (cpu_state->eax)
    {
    case 1:
        terminate_process(p);
    case 2:
        FB::putchar(*(char*)cpu_state->esi);
        break;
    case 3:
        PIC::disable_preemptive_scheduling(); // Is it necessary ? Isn't timer interrupt disabled when this runs ?
        start_process(cpu_state);
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
        dynlk(cpu_state);
        break;
    case 8:
        malloc(p, &p->cpu_state);
        break;
    case 9:
        free(p, &p->cpu_state);
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
    default:
        printf_error("Received unknown syscall id: %u", cpu_state->eax);
        break;
    }

    // Todo: Check if process ESP should be reset to process kernel stack top here
    Interrupts::resume_user_process_asm(p->cpu_state, p->stack_state);
}

void Syscall::mkdir(cpu_state_t* cpu_state)
{
    const char* path = (const char*)cpu_state->edi;

    cpu_state->eax = (uint)VFS::mkdir(path);
}

void Syscall::touch(cpu_state_t* cpu_state)
{
    const char* path = (const char*)cpu_state->edi;

    cpu_state->eax = (uint)VFS::touch(path);
}

void Syscall::ls(cpu_state_t* cpu_state)
{
    const char* path = (const char*)cpu_state->edi;

    cpu_state->eax = (uint)VFS::ls(path);
}
