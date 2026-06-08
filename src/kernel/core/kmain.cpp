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
//Todo: Free ELFs after programs termination
//Todo: Handle userland memory leaks
//Todo: Fix waitpid return value so that it is usable with macros such as WEXITSTATUS
//Todo: Fast kernel memory pool: constantly allocated memory region with super-fast allocator for tmp operations
//Todo: Syscall concurrent safety (that's a goddamn huge task)
//Todo: Check if userland programs can manipulate kernel space addresses througout syscalls (ex: write(1, KERNEL_VIRTUAL_ADDRESS_BASE), 10)
//Todo: Set proper wstatus values (signals, core dumps...). See 'man 2 wait'
//Todo: run 42sh in interactive mode (so that $PWD is preserved across commands)
//Todo: add a list remove method that takes an iterator as parameter
//Todo: Try to add gdb debugging support for userland programs
//Todo: compile and link libstdc++ dynamically
//Todo: pass every syscall to Linux parameter passing convention
//Todo: unify mlibc and brebos syscalls numbers (via header file)
//Todo: unify allocation methods (mmap and malloc) + use uniformly flags and prot across the whole memory code
// Todo: Some TERM or CORE signals should be catchable by processes. For example, SIGTERM simply asks processes
// to shut down, and OS terminates them only after a while if the process does not do it by itself
// Todo: implement Process::mmap_allocations using RB tree
// Todo: preload libc.so, lm.so and ld.so to accelerate process loading
// Todo: Update toolchain so that dynlk is registered automatically as a dependence of dynamically linked programs
// Todo: Proper address space management system for ELFLoader
// Todo: Load ELF dependencies when loading an ELF to GDB, to be able to debug ld or libc for example
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
