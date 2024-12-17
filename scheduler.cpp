#include "scheduler.h"

#include "dentry.h"
#include "PIT.h"
#include "system.h"
#include "GDT.h"
#include "VFS.h"
#include "clib/stdio.h"

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
		{
			free_terminated_process(*proc);
			ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
			ready_queue.count--;
		}
		else if (proc->is_waiting_key())
		{
			ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
			ready_queue.count--;
			waiting_queue.arr[(waiting_queue.start + waiting_queue.count) % MAX_PROCESSES] = proc->get_pid();
			waiting_queue.count++;
		}
		else
		{
			p = proc;

			// Update queue
			if (p->quantum)
				p->quantum -= CLOCK_TICK_MS;
			else
			{
				ready_queue.arr[(ready_queue.start + ready_queue.count) % MAX_PROCESSES] = p->get_pid();
				RESET_QUANTUM(p);
				ready_queue.start = (ready_queue.start + 1) % MAX_PROCESSES;
			}
		}
	}

	return p;
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

	page_table_t* page_tables = get_page_tables();

	// Set TSS esp0 to point to the syscall handler stack (i.e. tell the CPU where is syscall handler stack)
	GDT::set_tss_kernel_stack(p->k_stack_top);

	// Use process' address space
	Interrupts::change_pdt_asm(PHYS_ADDR((uint) &p->pdt));

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

	Process* proc = Process::from_memory(get_grub_modules()[module].start_addr, pid, ppid, argc, argv);
	if (!proc)
		return;

	processes[pid] = proc;
	set_process_ready(proc);
}

bool Scheduler::exec(const char* path, pid_t ppid, int argc, const char** argv)
{
	if (ready_queue.count == MAX_PROCESSES)
	{
		printf_error("Max process are already running");
		return false;
	}
	pid_t pid = get_free_pid();
	if (pid == MAX_PROCESSES)
	{
		printf_error("No more PID available");
		return false;
	}

	void* buf = VFS::load_file(path);
	if (!buf)
		return false;

	Process* proc = Process::from_memory((uint) buf, pid, ppid, argc, argv);
	delete[] (char*) buf;
	if (!proc)
		return false;

	processes[pid] = proc;
	set_process_ready(proc);

	return true;
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
	for (uint i = 0; i < MAX_PROCESSES; ++i)
	{
		ready_queue.arr[i] = -1;
		processes[i] = 0x00;
	}
	ready_queue.start = ready_queue.count = 0;
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
		// Add process to ready queue
		pid_t pid = waiting_queue.arr[(waiting_queue.start + waiting_queue.count) % MAX_PROCESSES];
		ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = pid;

		processes[pid]->cpu_state.eax = (uint) key; // Return key
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

void Scheduler::free_terminated_process(Process& p)
{
	release_pid(p.pid);
	delete &p;
}
