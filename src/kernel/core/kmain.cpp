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

#define FPS 50

// Write message, flush framebuffer, execute operation, write ok
#define FLUSHED_FB_OK_OP(msg, op) { FB::write(msg); FB::flush(); (op); FB::ok(); }
// Write message, execute operation, write ok
#define FB_OK_OP(msg, op) { FB::write(msg); (op); FB::ok(); }

extern "C" void _init(void); // NOLINT(*-reserved-identifier)

//Todo: Add support for multiple dynamically linked libs (register dyn lib dependencies)
//Todo: Advanced memory freeing (do something when free_pages do not manage to have free_bytes < FREE_THRESHOLD)
//Todo: Process R_386_PC32 relocations
//Todo: task bar displaying time and the progress bar
extern "C" int kmain(uint ebx) // Ebx contains GRUB's multiboot structure pointer
{
    _init(); // Execute constructors
    Interrupts::disable_asm();

    Memory::init((multiboot_info_t*)ebx);

    // Initialize framebuffer, so that we can use it and printf calls don't crash, which is preferable :D
    FB::init(FPS);

    FLUSHED_FB_OK_OP("Setting up GDT\n", GDT::init());

    FLUSHED_FB_OK_OP("Remapping PIC\n", PIC::init())

    FLUSHED_FB_OK_OP("Setting up IDT\n", IDT::init());

    FLUSHED_FB_OK_OP("Enabling interrupts\n", Interrupts::enable_asm())

    // Activates preemptive scheduling.
    // At this point, a kernel initialization process is created and will be preempted like any other process.
    // It will execute the remaining instructions until Scheduler::stop_kernel_init_process() is called.
    // Multiple processes can now run concurrently
    FLUSHED_FB_OK_OP("Activating preemptive scheduling\n", Scheduler::init());

    // Start refreshing the display every frame
    Scheduler::start_kernel_process((void*)FB::refresh_loop);

#ifdef PROFILING
    Profiling::init();
#endif

    FB_OK_OP("Initialize network card and stack\n", Network::init());

    FB_OK_OP("Initializing Virtual File System\n", VFS::init());

    Network::run();

    // Set terminal process ready
    Scheduler::exec("terminal", 0, 0, nullptr);

    // This makes the kernel initialization process end itself, thus ending kernel initialization
    Scheduler::stop_kernel_init_process();

    // Not supposed to be executed, but we never know...
    System::shutdown();
}
