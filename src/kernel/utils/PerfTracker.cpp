#include "PerfTracker.h"

#include <kstring.h>

__attribute__ ((format (printf, 1, 2)))
extern int printf(const char* format, ...);
__attribute__ ((format (printf, 2, 3)))
extern int sprintf(char* str, const char* format, ...);

list<PerfTracker::track_entry> PerfTracker::track_entries = {};

__attribute__((no_instrument_function))
extern uint64_t rdtsc();

PerfAccumulator::accumulator_entry PerfAccumulator::accumulators[MAX_ACCUMULATORS] = {};
uint PerfAccumulator::num_accumulators = 0;

PerfTracker::PerfTracker(const char* name) : name(name), start_tick(rdtsc())
{
}

PerfTracker::~PerfTracker()
{
    const uint64_t end_tick = rdtsc();
    track_entries.add({std::move(name), start_tick, end_tick});
}

void PerfTracker::report()
{
    if (track_entries.size() == 0)
    {
        printf("%s: no entries!", __PRETTY_FUNCTION__);
        PerfAccumulator::num_accumulators = 0;
        return;
    }

    const uint64_t total_duration = track_entries.get(track_entries.size() - 1)->end_tick - track_entries.begin()->start_tick;

    printf("Perf tracker report:\n");
    for (const auto& [name, start_tick, end_tick] : track_entries)
    {
        const uint64_t duration = end_tick - start_tick;
        const float percentage = 100.0 * static_cast<double>(duration) / total_duration;

        printf("- %s: %.2f%%\n", name.c_str(), percentage);
    }
    printf("Window: %u Kcycles\n", static_cast<uint>(total_duration / 1000));

    // Accumulators cover everything since the previous report, not only the window above, hence absolute values
    if (PerfAccumulator::num_accumulators)
        printf("Accumulators (since previous report):\n");
    // Two entries per line, so that the report fits on screen
    constexpr int COLUMN_WIDTH = 64;
    for (uint i = 0; i < PerfAccumulator::num_accumulators; i++)
    {
        const auto& [name, count, total_ticks] = PerfAccumulator::accumulators[i];
        char cell[128];
        int len;
        if (!total_ticks) // Plain counter
            len = sprintf(cell, "- %s: %u", name, static_cast<uint>(count));
        else
            len = sprintf(cell, "- %s: %u calls, %u Kc (avg %u)", name, static_cast<uint>(count),
                          static_cast<uint>(total_ticks / 1000), static_cast<uint>(total_ticks / count));

        const bool last_column = i % 2 == 1 || i == PerfAccumulator::num_accumulators - 1;
        if (!last_column)
            for (; len < COLUMN_WIDTH - 1; len++)
                cell[len] = ' ';
        cell[len] = '\0';
        printf(last_column ? "%s\n" : "%s ", cell);
    }

    track_entries.clear();
    PerfAccumulator::num_accumulators = 0;
}

PerfAccumulator::PerfAccumulator(const char* name) : name(name), start_tick(rdtsc())
{
}

PerfAccumulator::~PerfAccumulator()
{
    add(name, 1, rdtsc() - start_tick);
}

void PerfAccumulator::add(const char* name, uint64_t count, uint64_t ticks)
{
    uint i = 0;
    while (i < num_accumulators && strcmp(accumulators[i].name, name) != 0)
        i++;

    if (i == num_accumulators)
    {
        if (num_accumulators == MAX_ACCUMULATORS) [[unlikely]]
            return; // Silently drop, as printing from a page fault handler isn't an option
        accumulators[num_accumulators++] = {name, 0, 0};
    }

    accumulators[i].count += count;
    accumulators[i].total_ticks += ticks;
}
