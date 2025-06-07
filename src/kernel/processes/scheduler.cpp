#include "scheduler.h"

#include "ELFLoader.h"
#include "../core/PIT.h"
#include "../core/system.h"
#include "../core/GDT.h"
#include "../core/PIC.h"
#include "../core/fb.h"
#include "../file_management/VFS.h"
#include "sys/errno.h"

uint Scheduler::pid_pool = 0;
pid_t Scheduler::running_process = MAX_PROCESSES;
queue<pid_t, MAX_PROCESSES>* Scheduler::ready_queue{};
queue<pid_t, MAX_PROCESSES>* Scheduler::waiting_queue{};
Process* Scheduler::processes[MAX_PROCESSES] {};
MinHeap<Scheduler::asleep_process>* Scheduler::sleeping_processes{};
void* Scheduler::stack_switch_stack_top = nullptr;

/**
 * Switch to a new stack anc all a function (taking a process as parameter)
 * @param new_esp new stack pointer
 * @param fun function to call
 * @param p function parameter
 */
extern "C" [[noreturn]] void switch_stack_and_call_process_function(void* new_esp, void (*fun)(Process*), Process* p);

Process* Scheduler::get_next_process()
{
    Process* p = nullptr;
    running_process = MAX_PROCESSES;

    // Find next process to execute
    while (p == nullptr)
    {
        // Nothing to run
        if (ready_queue->empty())
            return nullptr;

        // Get process
        pid_t pid = running_process = ready_queue->getFirst();
        Process* proc = processes[pid];

        if (proc->is_terminated())
            relinquish_first_ready_process();
        else if (proc->is_waiting_key())
            set_first_ready_process_asleep_waiting_key_press();
        else if (proc->is_waiting_program())
            set_first_ready_process_asleep_waiting_process();
        else if (proc->is_sleeping())
            ready_queue->dequeue();
        else
        {
            if (proc->exec_running())
            {
                auto replacement = proc->exec_replacement;
                proc->set_flag(P_TERMINATED);
                relinquish_first_ready_process();
                processes[proc->pid] = replacement;
                proc = replacement;
            }

            // Update queue
            if (proc->quantum)
            {
                p = proc;
                p->quantum -= CLOCK_TICK_MS;
            }
            else
            {
                ready_queue->dequeue();
                ready_queue->enqueue(proc->get_pid());
                RESET_QUANTUM(proc);
            }
        }
    }

    return p;
}

void Scheduler::relinquish_first_ready_process()
{
    pid_t pid = running_process = ready_queue->getFirst();
    Process* proc = processes[pid];

    // Do not dequeue to prevent from excluding the exec replacement from the ready queue
    if (!(proc->flags & P_EXEC))
        ready_queue->dequeue();

    // If process has no parent, free it rith away, or if it has been replaced
    if (proc->ppid == proc->pid)
    {
        free_terminated_process(*proc);
        return;
    }

    // Nobody is waiting for this process to terminate, thus it's a zombie
    // If it has been terminated because of exec, free it right away
    if (!processes[proc->ppid]->is_waiting_for_any_child_to_terminate && !proc->is_waited_by_parent && !(proc->flags & P_EXEC))
        proc->set_flag(P_ZOMBIE);
    else // Someone is waiting for this process to end, thus we can actually end it
        free_terminated_process(*proc);
}

void Scheduler::set_first_ready_process_asleep_waiting_key_press()
{
    pid_t pid = running_process = ready_queue->dequeue();
    waiting_queue->enqueue(pid);
}

void Scheduler::set_first_ready_process_asleep_waiting_process()
{
    running_process = ready_queue->dequeue();
    // Nothing else to do, the rest has already been done in register_process_wait
}

void Scheduler::resume_process(Process* p)
{
    // Set TSS esp0 to point to the syscall handler stack (i.e. tell the CPU where is syscall handler stack)
    GDT::set_tss_kernel_stack(p->k_stack_top);

    // Use process' address space
    Interrupts::change_pdt_asm(PHYS_ADDR(Memory::page_tables, (uint) p->pdt));

    // Write some values in process' address space (likely syscall return values)
    for (const auto& pair : p->values_to_write)
        *(int*)pair.address = pair.value;

    // Safe to clear even though we use process address space since this data belongs to the kernel, and thus is in
    // higher half, which is shared by processes address spaces
    p->values_to_write.clear();

    // Jump
    if (p->flags & P_SYSCALL_INTERRUPTED)
    {
        p->flags &= ~P_SYSCALL_INTERRUPTED;
        Interrupts::resume_syscall_handler_asm(&p->k_cpu_state, &p->k_stack_state);
    }
    else
        Interrupts::resume_user_process_asm(&p->cpu_state, &p->stack_state);
}

int Scheduler::execve(Process* p, const char* path, int argc, const char** argv)
{
    Process* proc;
    if (!((proc = load_process(path, p->pid, p->ppid, argc, argv))))
        return -1;
    p->execve_transfer(proc);
    p->set_flag(P_EXEC);

    TRIGGER_TIMER_INTERRUPT;

    __builtin_unreachable();
}

[[noreturn]]
void Scheduler::schedule()
{
    // Wake up processes that have been sleeping enough
    check_for_processes_to_wake_up();

    Process* p = get_next_process();

    if (p == nullptr)
    {
        if (waiting_queue->empty() && sleeping_processes->empty())
            System::shutdown();
        else
        {
            // All processes are waiting for a key press. Thus, we can halt the CPU
            running_process = MAX_PROCESSES; // Indicate that no process is running

            __asm__ volatile("mov %0, %%esp" : : "r"(Memory::get_stack_top_ptr())); // Use global kernel stack
            __asm__ volatile("sti"); // Make sure interrupts are enabled
            __asm__ volatile("hlt"); // Halt
        }

        __builtin_unreachable();
    }

    if (!p) // Although theoretically impossible, this happens sometimes, I'd like to know why
        irrecoverable_error("%s: no process to run", __func__);

    switch_stack_and_call_process_function(stack_switch_stack_top, resume_process, p);
}

void Scheduler::start_module([[maybe_unused]] uint module, [[maybe_unused]] pid_t ppid, [[maybe_unused]] int argc, [[maybe_unused]] const char** argv)
{
    printf_error("%s: not adapted to new ELF Loaded", __func__);
    return;
    /*if (ready_queue->full())
    {
        printf_error("Max process are already running");
        return;
    }
    pid_t pid = get_free_pid();
    if (pid == MAX_PROCESSES)
    {
        printf_error("No more PID available");
        return;
    }

    char path[] = "GRUB_module_x";
    path[strlen(path) - 1] = '0' + module;
    Process* proc = ELFLoader::setup_elf_process(pid, ppid, argc, argv, path);
    if (!proc)
        return;

    processes[pid] = proc;
    set_process_ready(proc);*/
}

int Scheduler::exec(const char* path, pid_t ppid, int argc, const char** argv)
{
    pid_t pid = get_free_pid();
    if (pid == MAX_PROCESSES)
    {
        printf_error("No more PID available");
        return -1;
    }

    Process* proc;
    if (!((proc = load_process(path, pid, ppid, argc, argv))))
        return -1;

    processes[proc->pid] = proc;
    set_process_ready(proc);

    return proc->pid;
}


pid_t Scheduler::get_free_pid()
{
    for (uint i = 0; i < MAX_PROCESSES; ++i)
    {
        if (!(pid_pool & (1 << i)))
        {
            pid_pool |= (1 << i); // Mark PID as used
            return i;
        }
    }

    return MAX_PROCESSES; // No PID available
}

uint32_t get_eflags() {
    uint32_t eflags;
    asm volatile (
        "pushf\n\t"      // Push EFLAGS to the stack
        "pop %0"         // Pop into variable
        : "=r" (eflags)
    );
    return eflags;
}

void Scheduler::init()
{
    PIT::init();
    Process::init();

    uint stack_switch_pe = Memory::get_free_pe();
    Memory::allocate_page<false>(stack_switch_pe);
    stack_switch_stack_top = (void*)((stack_switch_pe << 12) + PAGE_SIZE - sizeof(uint)); // Stack top is at the end of the page

    // Those have to be pointers because they cannot be instantiated at program start since dynamic memory allocation
    // is not available at this moment. However, it is ok to allocate them now.
    ready_queue = new queue<pid_t, MAX_PROCESSES>();
    waiting_queue = new queue<pid_t, MAX_PROCESSES>();
    sleeping_processes = new MinHeap<asleep_process>(MAX_PROCESSES);

    set_process_ready(Memory::kernel_process);

    PIC::enable_preemptive_scheduling();
}

void Scheduler::shutdown()
{
    delete ready_queue;
    delete waiting_queue;
}

pid_t Scheduler::get_running_process_pid()
{
    return running_process;
}

Process* Scheduler::get_running_process()
{
    return running_process == MAX_PROCESSES ? nullptr : processes[running_process];
}

void Scheduler::wake_up_key_waiting_processes(char key)
{
    for (uint i = 0; i < waiting_queue->getCount(); ++i)
    {
        // Add process to ready queue and remove it from waiting queue
        pid_t pid = waiting_queue->dequeue();
        ready_queue->enqueue(pid);

        processes[pid]->cpu_state.eax = (uint)key; // Return key
        processes[pid]->flags &= ~P_WAITING_KEY; // Clear flag
    }
}

void Scheduler::set_process_ready(Process* p)
{
    if (processes[p->pid] && processes[p->pid]->pid != p->pid)
        irrecoverable_error("%s: a different process is registered at this pid", __func__);
    processes[p->pid] = p;
    ready_queue->enqueue(p->pid);
    RESET_QUANTUM(processes[p->pid]);
}

void Scheduler::release_pid(pid_t pid)
{
    pid_pool &= ~(1 << pid);
}

void Scheduler::check_for_processes_to_wake_up()
{
    uint tick = PIT::get_tick();
    while (!sleeping_processes->empty())
    {
        auto sleeping_process = sleeping_processes->min();
        if (sleeping_process.end_tick <= tick)
        {
            // Add process to ready queue and remove it from sleeping list
            pid_t pid = sleeping_process.process->pid;
            ready_queue->enqueue(pid);
            processes[pid]->flags &= ~P_SLEEPING; // Clear flag
            sleeping_processes->delete_min();
        }
        else
            break;
    }
}

Process* Scheduler::load_process(const char* path, pid_t pid, pid_t ppid, int argc, const char** argv)
{
    if (ready_queue->full())
    {
        printf_error("Max process are already running");
        return nullptr;
    }

    auto file = VFS::browse_to(path);
    if (!file)
        return nullptr;

    return ELFLoader::setup_elf_process(pid, ppid, argc, argv, file, 1);
}

void Scheduler::create_kernel_init_process(void* process_host_mem, const uint lowest_free_pe, Process** kernel_process)
{
    uint pid = get_free_pid();
    if (pid == MAX_PROCESSES)
        irrecoverable_error("No more PID available. Cannot finish kernel initialization.");

    // Manually construct a process for new Process() to work
    auto* kernel = (Process*)process_host_mem;
    kernel->page_tables = Memory::page_tables;
    kernel->pdt = Memory::pdt;
    kernel->mem_base = {.s = {&kernel->mem_base, 0}};
    kernel->freep = &kernel->mem_base;
    kernel->pid = pid;
    kernel->lowest_free_pe = lowest_free_pe;

    // Setup ourselves as if this was the actual kernel process
    running_process = pid;
    processes[pid] = kernel;
    *kernel_process = kernel;

    // Now we can properly construct the process
    stack_state_t dummy_stack_state{};
    auto k = new Process(0, nullptr, Memory::page_tables, Memory::pdt, &dummy_stack_state,
                         1, pid, pid, (uint)-1);

    // Transfer allocation information from dummy process to actual process
    k->lowest_free_pe = kernel->lowest_free_pe;
    k->mem_base = kernel->mem_base;
    Memory::memory_header* h;
    for (h = k->freep; h->s.ptr != &kernel->mem_base; h = h->s.ptr){}
    h->s.ptr = &k->mem_base; // Make free block list circular by removing the reference to the dummy process' mem_base
    k->free_bytes = k->free_bytes;

    // Register newly created process
    running_process = k->pid;
    processes[pid] = k;

    // Write result
    *kernel_process = k;
}

void Scheduler::wake_up_process_parent(pid_t process_pid)
{
    pid_t curr_pid = get_running_process_pid();
    pid_t ppid = processes[process_pid]->ppid;
    auto waiting_process = processes[ppid];

    // Resume process, unless it's the current process, in which case it is already in the ready queue
    if (ppid != curr_pid)
        ready_queue->enqueue(ppid);
    // waitpid return value
    if (auto wstatus_addr = (int*)waiting_process->cpu_state.esi)
        waiting_process->values_to_write.add({wstatus_addr, processes[process_pid]->ret_val});
    // Clear flag
    waiting_process->flags &= ~P_WAITING_PROCESS;
    // Set return value
    waiting_process->cpu_state.eax = process_pid;
    // Remove child
    waiting_process->children.remove(process_pid);
}

void Scheduler::free_terminated_process(Process& p)
{
    if (!(p.flags & P_TERMINATED) && !(p.flags & P_ZOMBIE))
        irrecoverable_error("Trying to free a process which is not terminated. PID: %d, flags: %d", p.pid, p.flags);

    // Resume all processes that were waiting for this process to terminate, unless the process has terminated
    // because of an exec. In such case, the resuming of waiting processes is delegated to the termination of the
    // exec replacement process.
    pid_t pid = p.pid;
    if (!(p.flags & P_EXEC))
    {
        if (p.is_waited_by_parent || processes[p.ppid]->is_waiting_for_any_child_to_terminate)
        {
            wake_up_process_parent(pid);
            p.is_waited_by_parent = processes[p.ppid]->is_waiting_for_any_child_to_terminate = false;
        }

        release_pid(p.pid);
        processes[p.pid] = nullptr;
    }

    // Make children processes orphans
    for (auto& child_id : p.children)
    {
        auto child = processes[child_id];
        child->ppid = child->pid; // No parent anymore
    }

    delete &p;
}

void Scheduler::stop_kernel_init_process()
{
    // Prune kernel process from ready queue
    PIC::disable_preemptive_scheduling();
    size_t n = ready_queue->getCount();
    for (size_t i = 0; i < n; i++)
    {
        auto pid = ready_queue->dequeue();
        if (pid != 0)
            ready_queue->enqueue(pid);
    }
    PIC::enable_preemptive_scheduling();

    // We asked for termination of kernel init process.
    // It is this precise process running those instructions.
    // If we asked for its termination, then it shouldn't do anything more once marked terminated.
    // Thus, we manually trigger the timer interrupt in order to call the scheduler,
    // select another process to be executed
    TRIGGER_TIMER_INTERRUPT
}

int Scheduler::register_process_wait(pid_t waiting_process, pid_t waited_for_process)
{
    Process* p = processes[waiting_process];

    // Waiting for any child
    if (waited_for_process == -1)
    {
        // If there is zombie child, free it and return immediately
        for (const auto& child : p->children)
        {
            if (processes[child] && processes[child]->flags & P_ZOMBIE)
            {
                auto pid = processes[child]->pid;
                free_terminated_process(*processes[child]);
                return pid; // Return the PID of the zombie child
            }
        }

        if (p->children.size() > 0)
        {
            // Register the wait for any child
            processes[waiting_process]->is_waiting_for_any_child_to_terminate = true;
            processes[waiting_process]->set_flag(P_WAITING_PROCESS);
            return 0;
        }
        return -ECHILD; // No child to wait for
    }

    if (!processes[waiting_process]->children.contains(waited_for_process))
        return -ECHILD;

    // Waiting for a zombie, ie a process waiting for someone to wait for it -> free it and return immediately
    if (processes[waited_for_process]->flags & P_ZOMBIE)
    {
        free_terminated_process(*processes[waited_for_process]);
        return waited_for_process;
    }

    // Register the wait and wait
    processes[waited_for_process]->is_waited_by_parent = true; // Register the waiting process

    processes[waiting_process]->set_flag(P_WAITING_PROCESS);
    return 0;
}

void Scheduler::set_process_asleep(Process* p, uint duration)
{
    p->set_flag(P_SLEEPING);
    uint num_ticks_to_wait = (uint)((float)TICKS_PER_SEC * (float)duration / 1000.f);
    if (!num_ticks_to_wait)
        num_ticks_to_wait = 1;
    uint end_tick = PIT::get_tick() + num_ticks_to_wait;
    sleeping_processes->insert({p, end_tick});
}

void Scheduler::start_kernel_process(void* eip)
{
    auto pid = get_free_pid();
    if (pid == MAX_PROCESSES)
        irrecoverable_error("No more PID available. Cannot finish kernel initialization.");

    // Allocate stack page
    auto stack_top_page_id = Memory::get_free_pe();
    uint k_stack_top = (stack_top_page_id << 12) + PAGE_SIZE - 4;
    Memory::allocate_page<false>(stack_top_page_id);
    stack_state_t stack_state{};

    // Create process
    auto p = new Process(0, nullptr, Memory::page_tables, Memory::pdt, &stack_state,
                         2, pid, pid, k_stack_top);

    // Setup process to mimic kernel_process
    p->lowest_free_pe = Memory::kernel_process->lowest_free_pe;

    // Setup PCB
    p->k_stack_state.eip = (uint)eip;
    p->k_stack_state.esp = k_stack_top;
    __asm__ volatile("mov %%ss, %0" : "=r"(p->k_stack_state.ss));
    __asm__ volatile("mov %%cs, %0" : "=r"(p->k_stack_state.cs));
    p->k_stack_state.eflags = get_eflags();

    // For the process to be recognized as a kernel process
    p->set_flag(P_SYSCALL_INTERRUPTED);

    // Register process as ready
    processes[pid] = p;
    set_process_ready(p);
}
