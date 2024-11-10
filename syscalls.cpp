#include "syscalls.h"
#include "interrupts.h"
#include "scheduler.h"
#include "clib/stdio.h"
#include "system.h"
#include "PIC.h"
#include "FAT.h"

void Syscall::start_process(cpu_state_t* cpu_state)
{
	// Load child process and set it ready
	Scheduler::start_module(cpu_state->ebx, Scheduler::get_running_process_pid(), cpu_state->ecx,
							(const char**) cpu_state->edx);
}

void Syscall::printf(cpu_state_t* cpu_state)
{
	printf_syscall((char*) cpu_state->ebx, (char*) cpu_state->ecx);
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
	cpu_state->eax = (uint) p->allocate_dyn_memory(cpu_state->ebx);
}

void Syscall::free(Process* p, cpu_state_t* cpu_state)
{
	p->free_dyn_memory((void*) cpu_state->ebx);
}

void Syscall::dynlk(cpu_state_t* cpu_state)
{
	// Get calling process and verify its identity is as expected
	Process* p = Scheduler::get_running_process();
	if ((uint) p != cpu_state->ecx)
		printf_error("running process address does not match calling process address");

	// Get relocation table
	Elf32_Rel* reloc_table = p->elf->get_rel_plt();
	if (reloc_table == nullptr)
	{
		printf_error("no reloc table");
		terminate_process(p);
	}

	// Check required symbol's relocation
	Elf32_Rel* rel = (Elf32_Rel*) ((uint) reloc_table + cpu_state->ebx);
	uint symbol = ELF32_R_SYM(rel->r_info);
	uint type = ELF32_R_TYPE(rel->r_info);
	if (type != R_386_JMP_SLOT)
	{
		printf_error("Unhandled relocation type: %d", type);
		terminate_process(p);
	}

	// Get dynsym and its string table
	Elf32_Shdr* dynsym_hdr = p->elf->get_dynsym_hdr();
	if (dynsym_hdr == 0x00)
	{
		printf_error("Where tf is dynsym ?");
		terminate_process(p);
	}

	Elf32_Shdr* strtab_h = ((Elf32_Shdr*) ((uint) p->elf->elf32Shdr +
										   p->elf->elf32Ehdr->e_shentsize * dynsym_hdr->sh_link));
	char* strtab = (char*) (p->elf->mod->start_addr + strtab_h->sh_offset);

	// Check symbol index makes sense
	uint dynsym_num_entries = dynsym_hdr->sh_size / dynsym_hdr->sh_entsize;
	if (symbol + 1 > dynsym_num_entries)
		printf_error("Required symbol has index %d while symtab only contains %d entries", symbol,
					 dynsym_num_entries);

	// Get symbol name
	Elf32_Sym* s = (Elf32_Sym*) (p->elf->mod->start_addr + dynsym_hdr->sh_offset +
								 symbol * dynsym_hdr->sh_entsize);
	char* symbol_name = &strtab[s->st_name];

	// Get lib
	GRUB_module* lib_mod = &get_grub_modules()[3];
	ELF lib_elf(lib_mod);

	// Get symbol in lib
	Elf32_Sym* lib_s = lib_elf.get_symbol(symbol_name);
	if (lib_s == nullptr)
	{
		printf_error("Symbol %s not found in klib", symbol_name);
		terminate_process(p);
	}

	// Compute symbol address
	uint lib_runtime_start_addr =
			(Scheduler::get_running_process()->elf->get_highest_runtime_addr() + (PAGE_SIZE - 1)) &
			~(PAGE_SIZE - 1);
	void* symbol_addr = (void*) (lib_runtime_start_addr + lib_s->st_value);

	*((void**) rel->r_offset) = symbol_addr; // Write symbol address to GOT
	Scheduler::get_running_process()->cpu_state.eax = (uint) symbol_addr; // Return symbol address
}

void Syscall::get_key()
{
	Scheduler::get_running_process()->set_flag(P_WAITING_KEY);

	__asm__ volatile("int $0x20");
}

[[noreturn]] void Syscall::dispatcher(cpu_state_t* cpu_state, struct stack_state* stack_state)
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
			printf(cpu_state);
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

	Interrupts::resume_user_process_asm(p->cpu_state, p->stack_state);
}

void Syscall::mkdir(cpu_state_t* cpu_state)
{
	uint drive_id = cpu_state->ebx;
	const char* path = (const char*) cpu_state->ecx;

	cpu_state->eax = (uint) FAT_drive::mkdir(drive_id, path);
}

void Syscall::touch(cpu_state_t* cpu_state)
{
	uint drive_id = cpu_state->ebx;
	const char* path = (const char*) cpu_state->ecx;

	cpu_state->eax = (uint) FAT_drive::touch(drive_id, path);
}

void Syscall::ls(cpu_state_t* cpu_state)
{
	uint drive_id = cpu_state->ebx;
	const char* path = (const char*) cpu_state->ecx;

	cpu_state->eax = (uint) FAT_drive::ls(drive_id, path);
}
