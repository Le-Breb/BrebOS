#include "process.h"

#include <kstring.h>

#include "ELFLoader.h"
#include "scheduler.h"
#include "../core/memory.h"
#include "../core/fb.h"

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
        if (!pte[i])
            continue;
        Memory::free_page(i * PAGE_SIZE, this);
    }

    children.clear();

    delete pte;
    delete elf_dependence_list;

    flags |= P_TERMINATED;

    //printf_info("Process %u exited with code %d", pid, ret_val);
}

Process::Process(uint num_pages, ELF* elf, Elf32_Addr runtime_load_address, const char* path) : quantum(0), priority(0),
    num_pages(num_pages), pte((uint*)::calloc(num_pages, sizeof(uint))),
    pid(MAX_PROCESSES), ppid(MAX_PROCESSES), k_stack_top(-1), flags(P_READY), lowest_free_pe(num_pages),
    elf_dependence_list(new struct elf_dependence_list(path, elf, runtime_load_address))
{
}


void Process::terminate(int ret_val)
{
    flags |= P_TERMINATED;
    this->ret_val = ret_val;
}

void* Process::malloc(uint n)
{
    return ::malloc(n, this);
}

void* Process::calloc(size_t nmemb, size_t size)
{
    return ::calloc(nmemb, size, this);
}

void* Process::realloc(void* ptr, size_t size)
{
    return ::realloc(ptr, size, this);
}

void Process::free(void* ptr)
{
    ::free(ptr, this);
}

void* Process::operator new(size_t size)
{
    return Memory::page_aligned_malloc(size);
}

void* Process::operator new[]([[maybe_unused]] size_t size)
{
    irrecoverable_error("Please do not use this operator");
    return (void*)1;
}

void Process::operator delete(void* p)
{
    Memory::page_aligned_free(p);
}

void Process::operator delete[]([[maybe_unused]] void* p)
{
    irrecoverable_error("do not call this");
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

uint Process::get_symbol_runtime_address(const struct elf_dependence_list* dep, const char* symbol_name)
{
    const Elf32_Sym* s = dep->elf->get_symbol(symbol_name);
    // Test whether the symbol exists, is defined in the ELF and has global visibility
    if (s && s->st_shndx && ELF32_ST_BIND(s->st_info) == STB_GLOBAL)
        return dep->runtime_load_address + s->st_value;

    while (dep)
    {
        s = dep->elf->get_symbol(symbol_name);
        if (s && s->st_shndx && ELF32_ST_BIND(s->st_info) == STB_GLOBAL)
            return dep->runtime_load_address + s->st_value;
        dep = dep->next; // Symbol not found in current ELF, proceed to next dependency
    }

    return 0x00;
}

void Process::init()
{
    // As the time I write this, strdup cannot be used as it uses malloc and malloc uses an interrupt in libc,
    // and firing interrupts while in kernel is not a great idea
    // So instead we gotta use this wonderful syntax...
    auto pwd = new env_var{};
    pwd->name = new char[4];
    memcpy((char*)pwd->name, "PWD", 4);
    pwd->value = new char[2];
    memcpy((char*)pwd->value, "/", 2);
    env_list.add(pwd);

    auto oldpwd = new env_var{};
    oldpwd->name = new char[7];
    memcpy((char*)oldpwd->name, "OLDPWD", 7);
    oldpwd->value = new char[2];
    memcpy((char*)oldpwd->value, "/", 2);

    auto home = new env_var{};
    home->name = new char[5];
    memcpy((char*)home->name, "HOME", 5);
    home->value = new char[3];
    memcpy((char*)home->value, " \t\n", 5);
    env_list.add(home);
}

char* Process::get_env(const char* name)
{
    for (auto i = 0; i < env_list.size(); i++)
    {
        auto e = *env_list.get(i);
        if (strcmp(name, e->name) == 0)
            return e->value;
    }

    return nullptr;
}

void Process::set_env(const char* name, const char* value)
{
    for (auto i = 0; i < env_list.size(); i++)
    {
        auto e = *env_list.get(i);
        if (strcmp(name, e->name) == 0)
        {
            delete e->value;
            e->value = strdup(value);
            return;
        }
    }

    env_list.add(new env_var{strdup(name), strdup(value)});
}

Process* Process::fork(pid_t child_pid)
{
    auto child = new Process(num_pages, elf_dependence_list->elf, 0, nullptr);
    memcpy(child->page_tables, page_tables, sizeof(page_tables));
    memcpy(child->pdt.entries, pdt.entries, sizeof(pdt.entries));
    child->quantum = quantum;
    child->priority = priority;
    memcpy(child->pte, pte, sizeof(uint) * num_pages);
    child->pid = child_pid;
    child->ppid = pid;

    child->flags = flags;

    return child;
}
