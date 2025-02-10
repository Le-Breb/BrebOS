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
ready_queue_t Scheduler::ready_queue;
ready_queue_t Scheduler::waiting_queue;
Process* Scheduler::processes[MAX_PROCESSES];

Process* Scheduler::get_next_process()
{
	Process* p = nullptr;
	running_process = MAX_PROCESSES;

	// Find next process to execute
	while (p == nullptr)
	{
		// Nothing to run
		if (ready_queue.count == 0)
			return nullptr;

		// Get process
		pid_t pid = running_process = ready_queue.arr[ready_queue.start];
		Process* proc = processes[pid];

		if (proc->is_terminated())
			relinquish_first_ready_process();
		else if (proc->is_waiting_key())
			set_first_ready_process_asleep_waiting_key_press();
		else
		{			// Update queue
			if (proc->quantum)
			{
				p = proc;
				p->quantum -= CLOCK_TICK_MS;
			}
			else
			{
				ready_queue.arr[ready_queue.start] = MAX_PROCESSES;
				ready_queue.arr[(ready_queue.start + ready_queue.count) % MAX_PROCESSES] = proc->get_pid();
				RESET_QUANTUM(proc);
				ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
			}
		}
	}

	return p;
}

void Scheduler::relinquish_first_ready_process()
{
	pid_t pid = running_process = ready_queue.arr[ready_queue.start];
	Process* proc = processes[pid];
	free_terminated_process(*proc);
	ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
	ready_queue.count--;
}

void Scheduler::set_first_ready_process_asleep_waiting_key_press()
{
	pid_t pid = running_process = ready_queue.arr[ready_queue.start];
	ready_queue.arr[ready_queue.start] = MAX_PROCESSES;
	ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
	ready_queue.count--;
	waiting_queue.arr[(waiting_queue.start + waiting_queue.count) % MAX_PROCESSES] = pid;
	waiting_queue.count++;
}

[[noreturn]] void Scheduler::schedule()
{
	Process* p = get_next_process();

	if (p == nullptr)
	{
		if (waiting_queue.count == 0)
			System::shutdown();
		else
		{
			// All processes are waiting for a key press. Thus, we can halt the CPU
			running_process = MAX_PROCESSES; // Indicate that no process is running

			__asm__ volatile("mov %0, %%esp" : : "r"(get_stack_top_ptr())); // Use global kernel stack
			__asm__ volatile("sti"); // Make sure interrupts are enabled
			__asm__ volatile("hlt"); // Halt
		}

		__builtin_unreachable();
	}

	// Set TSS esp0 to point to the syscall handler stack (i.e. tell the CPU where is syscall handler stack)
	GDT::set_tss_kernel_stack(p->k_stack_top);

	// Use process' address space
	Interrupts::change_pdt_asm(PHYS_ADDR(get_page_tables(), (uint) &p->pdt));

	// Jump
	if (p->flags & P_SYSCALL_INTERRUPTED)
	{
		p->flags &= ~P_SYSCALL_INTERRUPTED;
		Interrupts::resume_syscall_handler_asm(p->k_cpu_state, p->k_stack_state.esp - 12);
	}
	else
		Interrupts::resume_user_process_asm(p->cpu_state, p->stack_state);
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

	Process* proc = ELFLoader::setup_elf_process(get_grub_modules()[module].start_addr, pid, ppid, argc, argv);
	if (!proc)
		return;

	processes[pid] = proc;
	set_process_ready(proc);
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

	Process* proc = ELFLoader::setup_elf_process((uint)buf, pid, ppid, argc, argv);
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
	for (uint i = 0; i < MAX_PROCESSES; ++i)
	{
		ready_queue.arr[i] = MAX_PROCESSES;
		waiting_queue.arr[i] = MAX_PROCESSES;
		processes[i] = nullptr;
	}
	ready_queue.start = ready_queue.count = 0;

	// Create a process that represents the kernel itself
	// then continue kernel initialization with preemptive scheduling running
	create_kernel_init_process();
	PIC::enable_preemptive_scheduling();
}

pid_t Scheduler::get_running_process_pid()
{
	return running_process;
}

Process* Scheduler::get_running_process()
{
	return running_process == MAX_PROCESSES ? 0x00 : processes[running_process];
}

void Scheduler::wake_up_key_waiting_processes(char key)
{
	for (uint i = 0; i < waiting_queue.count; ++i)
	{
		// Add process to ready queue and remove it from waiting queue
		pid_t pid = waiting_queue.arr[(waiting_queue.start + waiting_queue.count - 1) % MAX_PROCESSES];
		waiting_queue.arr[(waiting_queue.start + waiting_queue.count - 1) % MAX_PROCESSES] = MAX_PROCESSES;
		ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = pid;

		processes[pid]->cpu_state.eax = (uint)key; // Return key
		processes[pid]->flags &= ~P_WAITING_KEY; // Clear flag
	}

	// Clear waiting queue
	waiting_queue.count = 0;
}

void Scheduler::set_process_ready(Process* p)
{
	ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = p->pid;
	RESET_QUANTUM(processes[p->pid]);
}

void Scheduler::release_pid(pid_t pid)
{
	pid_pool &= ~(1 << pid);
}

void Scheduler::create_kernel_init_process()
{
	Process* kernel = new Process(0);
	kernel->priority = 2;
	kernel->pdt = *get_pdt();
	uint pid = get_free_pid();
	kernel->pid = pid;
	if (pid == MAX_PROCESSES)
	{
		printf_error("No more PID available. Cannot finish kernel initialization.");
		System::shutdown();
	}
	processes[pid] = kernel;
	set_process_ready(kernel);
	running_process = pid;
}

void Scheduler::free_terminated_process(Process& p)
{
	release_pid(p.pid);
	ready_queue.arr[ready_queue.start] = MAX_PROCESSES;
	processes[p.pid] = nullptr;
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