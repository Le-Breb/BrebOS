#include "PerfTracker.h"

__attribute__ ((format (printf, 1, 2)))
extern int printf(const char* format, ...);

list<PerfTracker::track_entry> PerfTracker::track_entries = {};

__attribute__((no_instrument_function))
extern uint64_t rdtsc();

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
        return;
    }

    const uint64_t total_duration = track_entries.get(track_entries.size() - 1)->end_tick - track_entries.begin()->start_tick;

    printf("Perf tracker report:\n");
    for (const auto& [name, start_tick, end_tick] : track_entries)
    {
        const uint64_t duration = end_tick - start_tick;
        const float percentage = static_cast<double>(duration) / total_duration;

        printf("- %s: %.2f%%\n", name.c_str(), percentage);
    }

    track_entries.clear();
}
