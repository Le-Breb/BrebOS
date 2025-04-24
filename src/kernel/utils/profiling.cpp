#ifdef PROFILING

#include "profiling.h"

#include <kstdio.h>

#include "../core/fb.h"
#include "../core/memory.h"
#include "../file_management/VFS.h"

static bool initialized = false;
// 0xc0200000 is just an address higher than last but one entry of nm -n build/kernel.elf | grep ' T ' | sort -n
#define N (0xc0200000 - KERNEL_VIRTUAL_BASE)
static uint64_t* ncycles;
static uint64_t* start_cycle;


extern "C" void __attribute__((no_instrument_function))
__cyg_profile_func_enter([[maybe_unused]] void* this_fn, [[maybe_unused]] void* call_site)
{
    if (initialized)
    {
        auto fid = ((uint)this_fn - KERNEL_VIRTUAL_BASE);
        if (start_cycle[fid]) // Prevents recursion from overwriting the start time
            return;
        start_cycle[fid] = Profiling::rdtsc(); // Register start time (cycles)
    }
}

extern "C" void __attribute__((no_instrument_function))
__cyg_profile_func_exit([[maybe_unused]] void* this_fn, [[maybe_unused]] void* call_site)
{
    if (initialized)
    {
        auto fid = ((uint)this_fn - KERNEL_VIRTUAL_BASE);
        if (!start_cycle[fid])
            return;
        ncycles[fid] += Profiling::rdtsc() - start_cycle[fid]; // Write elapsed cycles
        start_cycle[fid] = 0; // Reset start time and allow for new measurements
    }
}

namespace Profiling
{
    void init()
    {
        // Calloc cannot be used here since it lazily allocates the pages. When this function returns,
        // func_exit is called, and will try to access the arrays. This will cause a page fault. The handler will
        // then call some other functions, which will call func_enter. func_enter will then try to access the array,
        // and now we enter an endless loop func_enter <-> page_fault handler
        auto size = N * sizeof(uint64_t);
        ncycles = (uint64_t*)malloc(size);
        memset(ncycles, 0, size);
        start_cycle = (uint64_t*)malloc(size);
        memset(start_cycle, 0, size);
        initialized = true;
    }

    void save_data()
    {
        initialized = false;
        // Count number of functions
        uint num_functions = 0;
        for (long unsigned int i = 0; i < N; i++)
        {
            if (!ncycles[i])
                continue;
            num_functions++;
        }

        // Write data to file. format is "function_address (8 bytes) cycles (16 bytes)\n"
        auto buf_len = (8 + 18) * num_functions;
        auto buf = new char[buf_len];
        auto buf_beg = buf;
        for (long unsigned int i = 0; i < N; i++)
        {
            if (!ncycles[i])
                continue;

            auto fun_addr = i + KERNEL_VIRTUAL_BASE;
            sprintf(buf, "%lx", fun_addr);
            buf += 8;
            sprintf(buf, " %016llu\n", ncycles[i]);
            buf += 18;
        }
        if (!VFS::write_buf_to_file("/prof.txt", buf_beg, buf_len))
            printf_error("Failed to write profiling data to file");

        initialized = true;
    }

    // Get current cycle count
    __attribute__((no_instrument_function))
    uint64_t rdtsc()
    {
        uint32_t lo, hi;
        asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
        return ((uint64_t)hi << 32) | lo;
    }
}

#endif