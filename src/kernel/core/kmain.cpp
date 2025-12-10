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
#define FLUSHED_FB_OK_OP(msg, op) { FB::write(msg); FB::flush(); (op); FB::ok(); FB::flush(); }
// Write message, execute operation, write ok
#define FB_OK_OP(msg, op) { FB::write(msg); (op); FB::ok(); }

extern "C" void _init(void); // NOLINT(*-reserved-identifier)

extern "C" bool fpu_init_asm_();

//Todo: Add support for multiple dynamically linked libs (register dyn lib dependencies)
//Todo: Advanced memory freeing (do something when free_pages do not manage to have free_bytes < FREE_THRESHOLD)
//Todo: task bar displaying time and the progress bar
//Todo: Use higher precision timer
//Todo: Free ELFs after programs termination
//Todo: Handle userland memory leaks
//Todo: Process R_386_PC32 relocations in order to rewrite libdynlk' Makefile without ld
//Todo: Fix waitpid return value so that it is usable with macros such as WEXITSTATUS
//Todo: Include newlib build in Makefile
extern "C" int kmain(uint ebx) // Ebx contains GRUB's multiboot2 structure pointer
{
    // Get why this fails
    // _init(); // Execute constructors

    Interrupts::disable_asm();

    Memory::init((multiboot_info_t*)ebx);

    // Initialize framebuffer, so that we can use it and printf calls don't crash, which is preferable :D
    FB::init(FPS);

    FLUSHED_FB_OK_OP("Setting up GDT\n", GDT::init());

    FLUSHED_FB_OK_OP("Remapping PIC\n", PIC::init())

    FLUSHED_FB_OK_OP("Setting up IDT\n", IDT::init());

    FLUSHED_FB_OK_OP("Enabling interrupts\n", Interrupts::enable_asm())

    if (!fpu_init_asm_())
        irrecoverable_error("FPU not available");

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
    const char* argv[2] = { "terminal", nullptr };
    Scheduler::exec("terminal", 0, 0, argv);

    // This makes the kernel initialization process end itself, thus ending kernel initialization
    Scheduler::stop_kernel_init_process();

    // Not supposed to be executed, but we never know...
    System::shutdown();
}
