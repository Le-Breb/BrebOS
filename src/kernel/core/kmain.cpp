#include "fb.h"
#include "GDT.h"
#include "interrupts.h"
#include "memory.h"
#include "system.h"
#include "../processes/scheduler.h"
#include "PIC.h"
#include "IDT.h"
#include "../file_management/VFS.h"
#include "../network/Network.h"
#include "../utils/profiling.h"

#define FPS 30 // Cannot go a lot higher since PIT is not precise enough

extern "C" void _init(void); // NOLINT(*-reserved-identifier)

//Todo: Add support for multiple dynamically linked libs (register dyn lib dependencies)
//Todo: Advanced memory freeing (do something when free_pages do not manage to have free_bytes < FREE_THRESHOLD)
//Todo: Process R_386_PC32 relocations
extern "C" int kmain(uint ebx) // Ebx contains GRUB's multiboot structure pointer
{
    _init(); // Execute constructors
    Interrupts::disable_asm();

    Memory::init((multiboot_info_t*)ebx);

    // Initialize framebuffer, so that we can use it and printf calls don't crash, which is preferable :D
    FB::init(FPS);

    FB::write("Setting up GDT\n");
    GDT::init();
    FB::ok();

    FB::write("Remapping PIC\n");
    PIC::init();
    FB::ok();

    FB::write("Setting up IDT\n");
    IDT::init();
    FB::ok();

    FB::write("Enabling interrupts\n");
    Interrupts::enable_asm();
    FB::ok();

    // Activates preemptive scheduling.
    // At this point, a kernel initialization process is created and will be preempted like any other process.
    // It will execute the remaining instructions until Scheduler::stop_kernel_init_process() is called.
    // Multiple processes can now run concurrently
    FB::write("Activating preemptive scheduling\n");
    Scheduler::init();
    FB::ok();

    // Start refreshing the display every frame
    Scheduler::start_kernel_process((void*)FB::refresh_loop);

#ifdef PROFILING
    Profiling::init();
#endif

    FB::write("Initialize network card and stack\n");
    Network::init();
    FB::ok();

    FB::write("Initializing Virtual File System\n");
    VFS::init();
    FB::ok();

    /*auto regs = list<IntV86::reg_val>();
    regs.add({IntV86::Reg16::AX, 0xCAFE});
    IntV86::interrupt(0x10, regs);*/

    Network::run();

    // Set terminal process ready
    Scheduler::exec("shell", 0, 0, nullptr);

    // This makes the kernel initialization process end itself, thus ending kernel initialization
    Scheduler::stop_kernel_init_process();

    // Not supposed to be executed, but we never know...
    System::shutdown();
}
