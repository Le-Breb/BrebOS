#include "scheduler.h"

#include "ELFLoader.h"
#include "thread.h"
#include "../core/PIT.h"
#include "../core/system.h"
#include "../core/GDT.h"
#include "../core/PIC.h"
#include "../core/fb.h"
#include "../file_management/VFS.h"

uint Scheduler::pid_pool = 0;
uint Scheduler::tid_pool = 0;
pid_t Scheduler::running_thread = MAX_PROCESSES;
ready_queue_t Scheduler::ready_queue;
ready_queue_t Scheduler::waiting_queue;
Process* Scheduler::processes[MAX_PROCESSES];
Thread* Scheduler::threads[MAX_THREADS];


Thread* Scheduler::get_next_thread()
{
    Thread* t = nullptr;
    running_thread = MAX_THREADS;

    // Find next thread to execute
    while (t == nullptr)
    {
        // Nothing to run
        if (ready_queue.count == 0)
            return nullptr;

        // Get process
        tid_t tid = running_thread = ready_queue.arr[ready_queue.start];
        Thread* thread = threads[tid];

        if (thread->is_terminated())
            relinquish_first_ready_process();
        else if (thread->is_waiting_key())
            set_first_ready_process_asleep_waiting_key_press();
        else
        {
            // Update queue
            if (thread->quantum)
            {
                t = thread;
                t->quantum -= CLOCK_TICK_MS;
            }
            else
            {
                ready_queue.arr[ready_queue.start] = MAX_PROCESSES;
                ready_queue.arr[(ready_queue.start + ready_queue.count) % MAX_PROCESSES] = thread->get_tid();
                RESET_QUANTUM(thread);
                ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
            }
        }
    }

    //printf_info("running thread with tid %u", t->get_tid());

    return t;
}

void Scheduler::relinquish_first_ready_process()
{
    tid_t tid = running_thread = ready_queue.arr[ready_queue.start];
    Thread* thread = threads[tid];
    free_terminated_process(*thread->get_process());
    ready_queue.start = (ready_queue.start + 1) % MAX_THREADS;
    ready_queue.count--;
}

void Scheduler::set_first_ready_process_asleep_waiting_key_press()
{
    tid_t tid = running_thread = ready_queue.arr[ready_queue.start];
    ready_queue.arr[ready_queue.start] = MAX_PROCESSES;
    ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
    ready_queue.count--;
    waiting_queue.arr[(waiting_queue.start + waiting_queue.count) % MAX_PROCESSES] = tid;
    waiting_queue.count++;
}

[[noreturn]] void Scheduler::schedule()
{
    Thread* t = get_next_thread();

    if (t == nullptr)
    {
        //printf_info("Ready queue : %u, Waiting queue : %u", ready_queue.count, waiting_queue.count);
        if (waiting_queue.count == 0)
            System::shutdown();
        else
        {
            // All processes are waiting for a key press. Thus, we can halt the CPU
            running_thread = MAX_PROCESSES; // Indicate that no thread is running
            //printf_info("All processes are waiting for a key press. Halting CPU");
            __asm__ volatile("mov %0, %%esp" : : "r"(get_stack_top_ptr())); // Use global kernel stack
            __asm__ volatile("sti"); // Make sure interrupts are enabled
            __asm__ volatile("hlt"); // Halt
        }

        __builtin_unreachable();
    }

    // Set TSS esp0 to point to the syscall handler stack (i.e. tell the CPU where is syscall handler stack)
    GDT::set_tss_kernel_stack(t->k_stack_top);

    // Use process' address space
    Interrupts::change_pdt_asm(PHYS_ADDR(get_page_tables(), (uint) &t->get_process()->pdt));

    // Jump
    if (t->flags & P_SYSCALL_INTERRUPTED)
    {
        t->flags &= ~P_SYSCALL_INTERRUPTED;
        Interrupts::resume_syscall_handler_asm(t->k_cpu_state, t->k_stack_state.esp - 12);
    }
    else
        Interrupts::resume_user_process_asm(t->cpu_state, t->stack_state);
}

void Scheduler::start_module(uint module, pid_t ppid, int argc, const char** argv)
{
    if (ready_queue.count == MAX_PROCESSES)
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
    Process* proc = ELFLoader::setup_elf_process(get_grub_modules()[module].start_addr, pid, ppid, argc, argv, path);
    if (!proc)
        return;

    processes[pid] = proc;
    set_thread_ready(proc->get_current_thread());
}

int Scheduler::exec(const char* path, pid_t ppid, int argc, const char** argv)
{
    if (ready_queue.count == MAX_PROCESSES)
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
    set_thread_ready(proc->get_current_thread());

    return pid;
}


uint Scheduler::get_free_spot(uint& values, uint max_value)
{
    for (uint i = 0; i < max_value; ++i)
    {
        if (!(values & (1 << i)))
        {
            values |= (1 << i); // Mark PID as used
            return i;
        }
    }

    return max_value;
}


pid_t Scheduler::get_free_pid()
{
    return get_free_spot(pid_pool, MAX_PROCESSES);
}

pid_t Scheduler::get_free_tid()
{
    return get_free_spot(tid_pool, MAX_THREADS);
}


void Scheduler::init()
{
    PIT::init();
    for (uint i = 0; i < MAX_PROCESSES; ++i)
    {
        processes[i] = nullptr;
    }
    for (uint i = 0; i < MAX_THREADS; ++i)
    {
        ready_queue.arr[i] = MAX_THREADS;
        waiting_queue.arr[i] = MAX_THREADS;
        threads[i] = nullptr;
    }
    ready_queue.start = ready_queue.count = 0;

    // Create a process that represents the kernel itself
    // then continue kernel initialization with preemptive scheduling running
    create_kernel_init_process();
    PIC::enable_preemptive_scheduling();
}

pid_t Scheduler::get_running_process_pid()
{
    return running_thread;
}

Thread* Scheduler::get_running_thread()
{
    return running_thread >= MAX_THREADS ? nullptr : threads[running_thread];
}

Thread **Scheduler::get_threads()
{
    return threads;
}


Process * Scheduler::get_running_process()
{
    if (get_running_thread() == nullptr)
        return nullptr;
    return get_running_thread()->get_process();
}

void Scheduler::wake_up_key_waiting_processes(char key)
{
    for (uint i = 0; i < waiting_queue.count; ++i)
    {
        // Add process to ready queue and remove it from waiting queue
        tid_t tid = waiting_queue.arr[(waiting_queue.start + waiting_queue.count - 1) % MAX_PROCESSES];
        waiting_queue.arr[(waiting_queue.start + waiting_queue.count - 1) % MAX_PROCESSES] = MAX_PROCESSES;
        ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = tid;

        threads[tid]->cpu_state.eax = (uint)key; // Return key
        threads[tid]->flags &= ~P_WAITING_KEY; // Clear flag
    }

    // Clear waiting queue
    waiting_queue.count = 0;
}

void Scheduler::set_thread_ready(Thread* t)
{
    ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = t->get_tid();
    RESET_QUANTUM(threads[t->get_tid()]);
}

void Scheduler::release_pid(pid_t pid)
{
    pid_pool &= ~(1 << pid);
}

void Scheduler::release_tid(tid_t tid)
{
    tid_pool &= ~(1 << tid);
}

void Scheduler::create_kernel_init_process()
{
    Process* kernel = new Process(0, nullptr, -1, "");
    kernel->priority = 2;
    kernel->pdt = *get_pdt();
    uint pid = get_free_pid();
    kernel->pid = pid;

    if (pid == MAX_PROCESSES)
    {
        printf_error("No more PID available. Cannot finish kernel initialization.");
        System::shutdown();
    }
    printf_info("Init process created with pid: %u and main thread tid: %u", pid, kernel->get_current_thread_tid());
    processes[pid] = kernel;
    set_thread_ready(kernel->get_current_thread());
    running_thread = kernel->get_current_thread_tid();
}

void Scheduler::free_terminated_process(Process& p)
{
    release_pid(p.pid);
    release_tid(p.get_current_thread_tid());

    ready_queue.arr[ready_queue.start] = MAX_THREADS;

    processes[p.pid] = nullptr;
    threads[p.get_current_thread_tid()] = nullptr;

    delete &p;
}

void Scheduler::stop_kernel_init_process()
{
    processes[0]->terminate();

    // We asked for termination of kernel init process.
    // It is this precise process running those instructions.
    // If we asked for its termination, then it shouldn't do anything more once marked terminated.
    // Thus, we manually trigger the timer interrupt in order to call the scheduler,
    // which will free the process and select another one to be executed
    TRIGGER_TIMER_INTERRUPT
}

tid_t Scheduler::add_thread(Process *process)
{
    tid_t tid = get_free_tid();
    threads[tid] = new Thread(process,tid);
    process->set_thread_pid(tid,threads[tid]);
    return tid;
}

