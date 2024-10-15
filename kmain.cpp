#include "fb.h"
#include "gdt.h"
#include "interrupts.h"
#include "multiboot.h"
#include "memory.h"
#include "system.h"
#include "scheduler.h"
#include "clib/stdio.h"
#include "PIT.h"
#include "PIC.h"
#include "IDT.h"

extern "C" void _init(void); // NOLINT(*-reserved-identifier)

//Todo: Make printf run mostly in user mode
//Todo: Make load_elf unload process if an error occurs
//Todo: Implement shared memory
//Todo: Investigate about whether page 184 (VGA buffer start address is mapped twice)
//Todo: same for page pde 771 pte11 at the end of run_module
extern "C" int kmain(unsigned int ebx)
{
	_init(); // Execute constructors
	Interrupts::disable_asm();

	// Initialize framebuffer
	FB::clear_screen();

	printf("Setting up GDT\n");
	GDT::init();
	FB::ok();

	printf("Remapping PIC\n");
	PIC::init();
	FB::ok();

	printf("Setting up IDT\n");
	IDT::init();
	FB::ok();

	printf("Initializing PIT\n");
	PIT::init();
	FB::ok();

	printf("Enabling interrupts\n");
	Interrupts::enable_asm();
	FB::ok();

	printf("Initialize memory ");
	init_mem((multiboot_info_t*) (ebx + 0xC0000000));
	FB::ok();

	printf("Setting up process handling\n");
	Scheduler::init();
	FB::ok();

	// Do stuff - make sure malloc and free work
	const size_t N = 100000;
	int* a = new int[N];
	for (unsigned int i = 0; i < N; i++)
		a[i] = 2;
	int* b = new int;
	*b = 2;

	delete b;
	delete[] a;

	// Set INIT process ready
	Scheduler::start_module(0, 0);

	// Run processes! :D
	PIC::enable_preemptive_scheduling();

	System::shutdown();
}
