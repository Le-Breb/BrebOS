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

//Todo: Advanced memory freeing (do something when free_pages do not manage to have free_bytes < FREE_THRESHOLD)
//Todo: Use higher precision timer
//Todo: Syscall concurrent safety (that's a goddamn huge task)
//Todo: Sanitize syscalls inputs (address ranges and permissions, value ranges)
//Todo: run 42sh in interactive mode (so that $PWD is preserved across commands)
//Todo: pass every syscall to Linux parameter passing convention
//Todo: unify mlibc and brebos syscalls numbers (via header file)
// Todo: Some TERM or CORE signals should be catchable by processes. For example, SIGTERM simply asks processes
// to shut down, and OS terminates them only after a while if the process does not do it by itself
// Todo: implement Process::mmap_allocations using RB tree
// Todo: add support for ELF versioning sections
// Todo: Investigate how vDSO could be implemented (mlibc has code related to that isn't it ?)
// Todo: Understand where did program loading delay came back from and get rid of it
// (cf. 18/06/26 screenshots where the last known fast loading project was, where a pull introduced delay back,
// and reverting the pull didn't remove the delay)
// Todo: use slab allocators for small allocation sizes (one allocator per size of 10 bytes ?)
// Todo: remove inheritance of memtree on bst since some inherited methods use new or delete and are thus invalid
// Todo: parse memory map from BIOS (to be aware of available regions and RAM size)
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
    const char* argv[2] = { "/bin/terminal", nullptr };
    const char* envp[] = { "MLIBC_DEBUG_PRINTF=0", "MLIBC_DEBUG_MALLOC=0", "NOPE_MLIBC_RTLD_DEBUG_VERBOSE=1", nullptr};
    Scheduler::exec("terminal", 0, 1, argv, envp);

    // This makes the kernel initialization process end itself, thus ending kernel initialization
    Scheduler::stop_kernel_init_process();

    // Not supposed to be executed, but we never know...
    System::shutdown();
}
