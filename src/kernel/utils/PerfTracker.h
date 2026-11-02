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

/**
 * Accumulates the time spent in a scope across all its executions, along with how many times it ran.
 * Meant for hot paths (page faults, syscalls...) where a PerfTracker entry per execution would flood the report.
 * Never allocates, so it is safe to use in code that can run inside malloc (e.g. page fault handlers).
 */
class PerfAccumulator
{
    const char* name;
    const uint64_t start_tick;

    struct accumulator_entry
    {
        const char* name;
        uint64_t count;
        uint64_t total_ticks;
    };
    static constexpr uint MAX_ACCUMULATORS = 64;
    static accumulator_entry accumulators[MAX_ACCUMULATORS];
    static uint num_accumulators;

    friend class PerfTracker;
public:
    explicit PerfAccumulator(const char* name);
    ~PerfAccumulator();

    /**
     * Adds count executions and ticks cycles to the name accumulator. With ticks = 0, it acts as a plain counter
     */
    static void add(const char* name, uint64_t count, uint64_t ticks = 0);
};

#define TRACK_PERF PerfTracker __tracker(__func__);
#define ACCUMULATE_PERF(name) PerfAccumulator __accumulator(name);