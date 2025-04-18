#include "scheduler.h"

#include "ELFLoader.h"
#include "../core/PIT.h"
#include "../core/system.h"
#include "../core/GDT.h"
#include "../core/PIC.h"
#include "../core/fb.h"
#include "../file_management/VFS.h"

uint Scheduler::pid_pool = 0;
pid_t Scheduler::running_process = MAX_PROCESSES;
queue<pid_t, MAX_PROCESSES>* Scheduler::ready_queue{};
queue<pid_t, MAX_PROCESSES>* Scheduler::waiting_queue{};
list<pid_t> Scheduler::process_waiting_list[MAX_PROCESSES] = {};
Process* Scheduler::processes[MAX_PROCESSES];

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
        else
        {
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
    pid_t pid = running_process = ready_queue->dequeue();
    Process* proc = processes[pid];

    // If process has no parent, free it rith away
    if (proc->ppid == proc->pid)
    {
        free_terminated_process(*proc);
        return;
    }

    // Nobody is waiting for this process to terminate, thus it's a zombie
    if (process_waiting_list[pid].size() == 0)
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

[[noreturn]] void Scheduler::schedule()
{
    Process* p = get_next_process();

    if (p == nullptr)
    {
        if (waiting_queue->empty())
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

    // Set TSS esp0 to point to the syscall handler stack (i.e. tell the CPU where is syscall handler stack)
    GDT::set_tss_kernel_stack(p->k_stack_top);

    // Use process' address space
    Interrupts::change_pdt_asm(PHYS_ADDR(Memory::page_tables, (uint) &p->pdt));

    // Write some values in process' address space (likely syscall return values)
    for (auto i = 0; i < p->values_to_write.size(); i++)
    {
        auto pair = *p->values_to_write.get(i);
        *(int*)pair.address = pair.value;
    }
    p->values_to_write.clear();

    // Jump
    if (p->flags & P_SYSCALL_INTERRUPTED)
    {
        p->flags &= ~P_SYSCALL_INTERRUPTED;
        Interrupts::resume_syscall_handler_asm(p->k_cpu_state, p->k_stack_state.esp - 12);
    }
    else
        Interrupts::resume_user_process_asm(&p->cpu_state, &p->stack_state);
}

void Scheduler::start_module(uint module, pid_t ppid, int argc, const char** argv)
{
    if (ready_queue->full())
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
    Process* proc = ELFLoader::setup_elf_process(Memory::grub_modules[module].start_addr, pid, ppid, argc, argv, path);
    if (!proc)
        return;

    processes[pid] = proc;
    set_process_ready(proc);
}

int Scheduler::exec(const char* path, pid_t ppid, int argc, const char** argv)
{
    if (ready_queue->full())
    {
        printf_error("Max process are already running");
        return -1;
    }
    pid_t pid = get_free_pid();
    if (pid == MAX_PROCESSES)
    {
        printf_error("No more PID available");
        return -1;
    }

    void* buf = VFS::load_file(path);
    if (!buf)
        return -1;

    Process* proc = ELFLoader::setup_elf_process((uint)buf, pid, ppid, argc, argv, path);
    delete[] (char*)buf;
    if (!proc)
        return -1;

    processes[pid] = proc;
    set_process_ready(proc);

    return pid;
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

void Scheduler::init()
{
    PIT::init();
    Process::init();
    memset(processes, 0, sizeof(processes));
    // Those have to be pointers because they cannot be instantiated at program start since dynamic memory allocation
    // is not available at this moment. However, it is ok to allocate them now.
    ready_queue = new queue<pid_t, MAX_PROCESSES>();
    waiting_queue = new queue<pid_t, MAX_PROCESSES>();

    // Create a process that represents the kernel itself
    // then continue kernel initialization with preemptive scheduling running
    create_kernel_init_process();
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
    ready_queue->enqueue(p->pid);
    RESET_QUANTUM(processes[p->pid]);
}

void Scheduler::release_pid(pid_t pid)
{
    pid_pool &= ~(1 << pid);
}

void Scheduler::create_kernel_init_process()
{
    Process* kernel = new Process(0, nullptr, -1, "");
    kernel->priority = 2;
    kernel->pdt = *Memory::pdt;
    uint pid = get_free_pid();
    kernel->pid = pid;
    kernel->ppid = pid; // No parent

    if (pid == MAX_PROCESSES)
        irrecoverable_error("No more PID available. Cannot finish kernel initialization.");

    processes[pid] = kernel;
    set_process_ready(kernel);
    running_process = pid;
}

void Scheduler::free_terminated_process(Process& p)
{
    if (!(p.flags & P_TERMINATED) && !(p.flags & P_ZOMBIE))
        irrecoverable_error("Trying to free a process which is not terminated. PID: %d, flags: %d", p.pid, p.flags);

    // Resume all processes that were waiting for this process to terminate
    pid_t pid = p.pid;
    pid_t curr_pid = get_running_process_pid();
    for (auto i = 0; i < process_waiting_list[pid].size(); i++)
    {
        auto waiting_process_pid = *process_waiting_list[pid].get(i);
        auto waiting_process = processes[waiting_process_pid];

        // Resume process, unless it's the current process, in which case it is already in the ready queue
        if (waiting_process_pid != curr_pid)
            ready_queue->enqueue(waiting_process_pid);
        // waitpid return value
        auto wstatus_addr = (int*)waiting_process->cpu_state.esi;
        if (wstatus_addr)
            waiting_process->values_to_write.add({wstatus_addr, processes[pid]->ret_val});
        // Clear flag
        waiting_process->flags &= ~P_WAITING_PROCESS;
        // Remove child
        waiting_process->children.remove(pid);
    }
    process_waiting_list[pid].clear();

    release_pid(p.pid);
    processes[p.pid] = nullptr;
    delete &p;
}

void Scheduler::stop_kernel_init_process()
{
    processes[0]->terminate(0);

    // We asked for termination of kernel init process.
    // It is this precise process running those instructions.
    // If we asked for its termination, then it shouldn't do anything more once marked terminated.
    // Thus, we manually trigger the timer interrupt in order to call the scheduler,
    // which will free the process and select another one to be executed
    TRIGGER_TIMER_INTERRUPT
}

bool Scheduler::register_process_wait(pid_t waiting_process, pid_t waiting_for_process)
{
    if (!processes[waiting_for_process])
        return false;

    process_waiting_list[waiting_for_process].add(waiting_process);

    // Waiting for a zombie, ie a process waiting for someone to wait for it -> free it
    if (processes[waiting_for_process]->flags & P_ZOMBIE)
        free_terminated_process(*processes[waiting_for_process]);
    else
        processes[waiting_process]->set_flag(P_WAITING_PROCESS);

    return true;
}
