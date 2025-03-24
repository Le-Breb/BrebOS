#ifdef PROFILING

#ifndef PROFILING_H
#define PROFILING_H
#include <kstdint.h>

namespace Profiling
{
    void save_data();

    __attribute__((no_instrument_function))
    uint64_t rdtsc();

    void init();
}
#endif //PROFILING_H

#endif