#include "fb.h"
#include "GDT.h"
#include "interrupts.h"
#include "../boot/multiboot.h"
#include "memory.h"
#include "system.h"
#include "../processes/scheduler.h"
#include "PIC.h"
#include "IDT.h"
#include "../file_management/VFS.h"

extern "C" void _init(void); // NOLINT(*-reserved-identifier)

//Todo: printf buffering and flushing
//Todo: Investigate about whether page 184 (VGA buffer start address is mapped twice)
//Todo: same for page pde 771 pte11 at the end of run_module
//Todo: Add support for multiple dynamically linked libs (register dyn lib dependencies)
//Todo: Advanced memory freeing (do something when free_pages do not manage to have free_bytes < FREE_THRESHOLD)
//Todo: Implement process schedule_timeout to have sleeping processes out of the scheduler ready queue (usage: sleep)
//Todo: Make allocate page user allocate below memory higher half, and above for kernel pages
//Todo: Process R_386_PC32 and R_386_32 relocations
extern "C" int kmain(uint ebx) // Ebx contains GRUB's multiboot structure pointer
{
	_init(); // Execute constructors
	Interrupts::disable_asm();

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

	printf("Enabling interrupts\n");
	Interrupts::enable_asm();
	FB::ok();

	printf("Initialize memory\n");
	init_mem((multiboot_info_t*)(ebx + KERNEL_VIRTUAL_BASE));
	FB::ok();

	// Activates preemptive scheduling.
	// At this point, a kernel initialization process is created and will be preempted like any other process.
	// It will execute the remaining instructions until Scheduler::stop_kernel_init_process() is called.
	// Multiple processes can now run concurrently
	printf("Activating preemptive scheduling\n");
	Scheduler::init();
	FB::ok();

	printf("Initializing Virtual File System\n");
	VFS::init();
	FB::ok();

	// Set INIT process ready
	Scheduler::exec("shell", 0, 0, nullptr);

	// This makes the kernel initialization process end itself, thus ending kernel initialization
	Scheduler::stop_kernel_init_process();

	// Not supposed to be executed, but we never know...
	System::shutdown();
}
