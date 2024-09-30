#include "syscalls.h"
#include "interrupts.h"
#include "process.h"
#include "clib/stdio.h"
#include "system.h"
#include "elf_tools.h"

/**
 * Starts a GRUB module as a child of current process
 * @param cpu_state CPU state
 * @param stack_state Stack state
 */
void syscall_start_process(cpu_state_t* cpu_state);

/**
 * Returns current process' PID  */
void syscall_get_pid();

/**
 * Prints a formatted string
 * @param cpu_state CPU state. EBX format string, ECX = pointer to printf arguments (on user stack)
 */
void syscall_printf(cpu_state_t* cpu_state);

/**
 * Terminates a process
 * @param p process to terminate
 */
_Noreturn void syscall_terminate_process(process* p);

void syscall_start_process(cpu_state_t* cpu_state)
{
	// Load child process and set it ready
	start_module(cpu_state->ebx, get_running_process_pid());
}

void syscall_printf(cpu_state_t* cpu_state)
{
	printf_syscall((char*) cpu_state->ebx, (char*) cpu_state->ecx);
}

void syscall_get_pid()
{
	process* running_process = get_running_process();

	running_process->cpu_state.eax = running_process->pid; // Return PID
}

_Noreturn void syscall_terminate_process(process* p)
{
	terminate_process(p);

	// We definitely do not want to continue as next step is to resume the process we just terminated !
	// Instead, we manually raise a timer interrupt which we schedule another process
	// (The terminated one as its terminated flag set, so it won't be selected by the scheduler)
	__asm__ volatile("int $0x20");

	// We will never resume the code here
	__builtin_unreachable();
}

void syscall_dynlk(cpu_state_t* cpu_state)
{
	// Get calling process and verify its identity is as expected
	process* p = get_running_process();
	if ((unsigned int) p != cpu_state->ecx)
		printf_error("running process address does not match calling process address");

	// Get relocation table
	Elf32_Rel* reloc_table = get_elf_rel_plt(p->elfi);
	if (reloc_table == 0x00)
	{
		printf_error("no reloc table");
		syscall_terminate_process(p);
	}

	// Check required symbol's relocation
	Elf32_Rel* rel = (Elf32_Rel*) ((unsigned int) reloc_table + cpu_state->ebx);
	unsigned int symbol = ELF32_R_SYM(rel->r_info);
	unsigned int type = ELF32_R_TYPE(rel->r_info);
	if (type != R_386_JMP_SLOT)
	{
		printf_error("Unhandled relocation type: %d", type);
		syscall_terminate_process(p);
	}

	// Get dynsym and its string table
	Elf32_Shdr* dynsym_hdr = get_elf_dynsym_hdr(p->elfi);
	if (dynsym_hdr == 0x00)
	{
		printf_error("Where tf is dynsym ?");
		syscall_terminate_process(p);
	}

	Elf32_Shdr* strtab_h = ((Elf32_Shdr*) ((unsigned int) p->elfi->elf32Shdr +
										   p->elfi->elf32Ehdr->e_shentsize * dynsym_hdr->sh_link));
	char* strtab = (char*) (p->elfi->mod->start_addr + strtab_h->sh_offset);

	// Check symbol index makes sense
	unsigned int dynsym_num_entries = dynsym_hdr->sh_size / dynsym_hdr->sh_entsize;
	if (symbol + 1 > dynsym_num_entries)
		printf_error("Required symbol has index %d while symtab only contains %d entries", symbol,
					 dynsym_num_entries);

	// Get symbol name
	Elf32_Sym* s = (Elf32_Sym*) (p->elfi->mod->start_addr + dynsym_hdr->sh_offset +
								 symbol * dynsym_hdr->sh_entsize);
	char* symbol_name = &strtab[s->st_name];

	// Get lib
	GRUB_module* lib_mod = &get_grub_modules()[3];
	elf_info lib_elfi = {(Elf32_Ehdr*) lib_mod->start_addr,
						 (Elf32_Phdr*) (lib_mod->start_addr + lib_elfi.elf32Ehdr->e_phoff),
						 (Elf32_Shdr*) (lib_mod->start_addr + lib_elfi.elf32Ehdr->e_shoff), lib_mod};

	// Get symbol in lib
	Elf32_Sym* lib_s = get_elf_symbol(&lib_elfi, symbol_name);
	if (lib_s == 0x00)
	{
		printf_error("Symbol %s not found in klib", symbol_name);
		syscall_terminate_process(p);
	}

	// Compute symbol address
	unsigned int lib_runtime_start_addr =
			(get_elf_highest_runtime_addr(get_running_process()->elfi) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
	void* symbol_addr = (void*) (lib_runtime_start_addr + lib_s->st_value);

	*((void**) rel->r_offset) = symbol_addr; // Write symbol address to GOT
	get_running_process()->cpu_state.eax = (unsigned int) symbol_addr; // Return symbol address
}

void syscall_get_key()
{
	process* p = get_running_process();
	p->flags |= P_WAITING_KEY;

	__asm__ volatile("int $0x20");
}

_Noreturn void syscall_handler(cpu_state_t* cpu_state, struct stack_state* stack_state)
{
	process* p = get_running_process();

	// Update PCB
	p->cpu_state = *cpu_state;
	p->stack_state = *stack_state;

	switch (cpu_state->eax)
	{
		case 1:
			syscall_terminate_process(p);
		case 2:
			syscall_printf(cpu_state);
			break;
		case 3:
			disable_preemptive_scheduling();
			syscall_start_process(cpu_state);
			enable_preemptive_scheduling();
			break;
		case 4:
			syscall_get_key();
			break;
		case 5:
			syscall_get_pid();
			break;
		case 6:
			shutdown();
		case 7:
			syscall_dynlk(cpu_state);
			break;
		default:
			printf_error("Received unknown syscall id: %u", cpu_state->eax);
			break;
	}

	resume_user_process_asm(p->cpu_state, p->stack_state);
}