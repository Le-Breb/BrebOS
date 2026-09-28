#pragma once

#include "list.h"
#include "string.h"
#include <stdint.h>

class PerfTracker
{
    const string name;
    const uint64_t start_tick;

    struct track_entry
    {
        string name;
        uint64_t start_tick;
        uint64_t end_tick;
    };
    static list<track_entry> track_entries;
public:
    PerfTracker(const char* name);
    ~PerfTracker();

    static void report();
};

#define TRACK_PERF PerfTracker tracker(__func__);