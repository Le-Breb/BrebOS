#include "fb.h"
#include "GDT.h"
#include "interrupts.h"
#include "multiboot.h"
#include "memory.h"
#include "system.h"
#include "scheduler.h"
#include "clib/stdio.h"
#include "PIT.h"
#include "PIC.h"
#include "IDT.h"
#include "ATA.h"
#include "FAT.h"

extern "C" void _init(void); // NOLINT(*-reserved-identifier)

//Todo: Try to figure out address 0 page faulting (like isn't proc entry point there ?)
//Todo: Kill process when major issue occurs (such as PAGEFAULT)
//Todo: Make printf run mostly in user mode
//Todo: Implement shared memory
//Todo: Investigate about whether page 184 (VGA buffer start address is mapped twice)
//Todo: same for page pde 771 pte11 at the end of run_module
extern "C" int kmain(unsigned int ebx)
{
	_init(); // Execute constructors
	Interrupts::disable_asm();

	// Initialize framebuffer
	FB::init();

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

	printf("Initializing FAT drives and ATA driver\n");
	FAT_drive::init();
	FB::ok();

	printf("Setting up process handling\n");
	Scheduler::init();
	FB::ok();


	/*if (!FAT_drive::mkdir(0, "/afolder"))
		printf_error("error creating /afolder");
	else if (!FAT_drive::mkdir(0, "/afolder/bam"))
		printf_error("error creating /afolder/bam");
	else if (!FAT_drive::mkdir(0, "/afolder/bam/badaboum"))
		printf_error("error creating /afolder/bam/badaboum");
	else if (!FAT_drive::touch(0, "/afolder/bam/touch.txt"))
		printf_error("error creating /afolder/bam/touch.txt");
	if (!FAT_drive::ls(0, "/"))
		printf_error("ls error");*/

	// Set INIT process ready
	Scheduler::start_module(0, 0);

	// Run processes! :D
	PIC::enable_preemptive_scheduling();

	// Not supposed to be executed, but we never know...
	System::shutdown();
}