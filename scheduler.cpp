#include "scheduler.h"
#include "PIT.h"
#include "system.h"
#include "GDT.h"
#include "clib/stdio.h"
#include "clib/string.h"

unsigned int Scheduler::pid_pool = 0;
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
			free_terminateed_process(*proc);
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
	Interrupts::change_pdt_asm(PHYS_ADDR((unsigned int) &p->pdt));

	// Jump
	if (p->flags & P_SYSCALL_INTERRUPTED)
	{
		p->flags &= ~P_SYSCALL_INTERRUPTED;
		Interrupts::resume_syscall_handler_asm(p->k_cpu_state, p->k_stack_state.esp - 12);
	}
	else
		Interrupts::resume_user_process_asm(p->cpu_state, p->stack_state);
}

void Scheduler::start_module(unsigned int module, pid_t ppid, int argc, const char** argv)
{
	if (ready_queue.count == MAX_PROCESSES)
	{
		printf_error("Max process are already running");
		return;
	}

	pid_t pid = Scheduler::get_free_pid();
	if (pid == MAX_PROCESSES)
	{
		printf_error("No more PID available");
		return;
	}

	GRUB_module* grub_modules = get_grub_modules();

	// Check whether file is an elf
	Elf32_Ehdr* elf32Ehdr = (Elf32_Ehdr*) grub_modules[module].start_addr;
	unsigned int elf = elf32Ehdr->e_ident[0] == 0x7F && elf32Ehdr->e_ident[1] == 'E' && elf32Ehdr->e_ident[2] == 'L' &&
					   elf32Ehdr->e_ident[3] == 'F';

	// Load process into memory
	Process* proc = elf ? Process::from_ELF(&grub_modules[module]) : Process::from_binary(module, grub_modules);

	// Ensure process was loaded successfully
	if (proc == nullptr)
	{
		printf_error("Error while loading program. Aborting");
		return;
	}

	// Allocate process stack page
	unsigned int p_stack_pe_id = get_free_pe();
	allocate_page_user(get_free_page(), p_stack_pe_id);
	unsigned int p_stack_top_v_addr = p_stack_pe_id * PAGE_SIZE + PAGE_SIZE;

	// Allocate syscall handler stack page
	unsigned int k_stack_pe = get_free_pe();
	unsigned int k_stack_pde = k_stack_pe / PDT_ENTRIES;
	unsigned int k_stack_pte = k_stack_pe % PDT_ENTRIES;
	allocate_page(get_free_page(), k_stack_pe);

	// Set process PDT entries: entries 0 to 767 map the process address space using its own page tables
	// Entries 768 to 1024 point to kernel page tables, so that kernel is mapped. Moreover, syscall handlers
	// will not need to switch to kernel pdt to make changes in kernel memory as it is mapped the same way
	// in every process' PDT
	// Set process pdt entries to target process page tables
	page_table_t* page_tables = get_page_tables();
	for (int i = 0; i < 768; ++i)
		proc->pdt.entries[i] = PHYS_ADDR((unsigned int) &proc->page_tables[i]) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;
	// Use kernel page tables for the rest
	for (int i = 768; i < PDT_ENTRIES; ++i)
		proc->pdt.entries[i] = PHYS_ADDR((unsigned int) &page_tables[i]) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;

	// Map process code at 0x00
	for (unsigned int i = 0; i < proc->num_pages; ++i)
	{
		unsigned int page_pde = i / PDT_ENTRIES;
		unsigned int page_pte = i % PDT_ENTRIES;

		unsigned int process_page_id = proc->pte[i];
		unsigned int pte = PTE(process_page_id);

		if (proc->page_tables[page_pde].entries[page_pte] != 0)
			printf_error("Non empty pt entry");
		proc->page_tables[page_pde].entries[page_pte] = pte;
	}

	// Map process stack at 0xBFFFFFFC = 0xCFFFFFFF - 4 at pde 767 and pte 1023, just below the kernel
	if (proc->page_tables[767].entries[1023] != 0)
		printf_error("Process kernel page entry is not empty");
	proc->page_tables[767].entries[1023] = PTE(p_stack_pe_id);

	// Setup PCB
	processes[pid] = proc;
	proc->pid = pid;
	proc->ppid = ppid;
	proc->priority = 1;
	proc->k_stack_top = VIRT_ADDR(k_stack_pde, k_stack_pte, (PAGE_SIZE - sizeof(int)));
	//proc->stack_state.eip = 0;
	proc->stack_state.ss = 0x20 | 0x03;
	proc->stack_state.cs = 0x18 | 0x03;
	proc->stack_state.esp = proc->write_args_to_stack(p_stack_top_v_addr, argc, argv);
	proc->stack_state.eflags = 0x200;
	proc->stack_state.error_code = 0;
	memset(&proc->cpu_state, 0, sizeof(proc->cpu_state));

	// Set process ready
	Scheduler::set_process_ready(proc);
}


pid_t Scheduler::get_free_pid()
{
	for (unsigned int i = 0; i < MAX_PROCESSES; ++i)
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
	for (unsigned int i = 0; i < MAX_PROCESSES; ++i)
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
	for (unsigned int i = 0; i < waiting_queue.count; ++i)
	{
		// Add process to ready queue
		pid_t pid = waiting_queue.arr[(waiting_queue.start + waiting_queue.count) % MAX_PROCESSES];
		ready_queue.arr[(ready_queue.start + ready_queue.count++) % MAX_PROCESSES] = pid;

		processes[pid]->cpu_state.eax = (unsigned int) key; // Return key
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

void Scheduler::free_terminateed_process(Process& p)
{
	release_pid(p.pid);
	delete &p;
}
