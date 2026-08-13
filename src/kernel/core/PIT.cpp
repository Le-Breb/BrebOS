#include "PIT.h"
#include "IO.h"
#include "../processes/scheduler.h"
#include "system.h"

uint32_t PIT::tsc_ticks_per_us = 0;

uint PIT::get_tick()
{
    return ticks;
}

uint16_t pit_read_counter()
{
    outb(PIT_COMMAND_PORT, 0b00000000); // Channel 0, latch count

    uint8_t lo = inb(PIT_CHANNEL0_PORT);
    uint8_t hi = inb(PIT_CHANNEL0_PORT);

    return ((uint16_t)hi << 8) | lo;
}

/**
 * Reads channel 0's status byte through the read-back command. The bit we care about is Null
 * Count: the PIT sets it when a reload value has been written but not yet transferred into the
 * counting element, and clears it once the transfer happens (one input clock, ~838ns, later).
 */
uint8_t pit_read_status()
{
    // Read-back command: latch status only (bit 5 set = don't latch the count), channel 0
    outb(PIT_COMMAND_PORT, 0b11100010);

    return inb(PIT_CHANNEL0_PORT);
}

void pit_set_reload_value(uint16_t value, uint8_t mode)
{
    // Command byte: channel 0, access lobyte/hibyte, given mode, binary counting
    outb(PIT_COMMAND_PORT, 0b00110000 | mode);

    outb(PIT_CHANNEL0_PORT, value & 0xFF); // Send low byte of divider
    outb(PIT_CHANNEL0_PORT, (value >> 8) & 0xFF); // Send high byte of divider
}

uint32_t PIT::calibrate_tsc()
{
    // Rather than assume the measurement covered a fixed window, both endpoints of the PIT counter
    // are sampled and the elapsed time derived from the difference. That makes the result immune to
    // when the window actually opened and closed: if a stall (firmware SMI activity, say) delays
    // us, it inflates the TSC delta and the PIT delta alike and the ratio still holds.
    constexpr uint16_t start_count = 0xFFFF;

    // Stop short of 0 so the counter can't run past the end of the window and wrap while we're
    // sampling it. The remaining span is still ~53ms, far more than needed for a stable ratio.
    constexpr uint16_t end_count = 0x1000;

    for (int attempt = 0; attempt < 5; attempt++)
    {
        // Mode 0: one decrement per input clock. Mode 3 (what the kernel tick uses) decrements by
        // two, which would silently halve the real duration of the window measured below.
        pit_set_reload_value(start_count, PIT_MODE_ONE_SHOT);

        // Writing the reload value does not load the counting element straight away. Until it does,
        // the counter still reads whatever it held before - so sampling now would compare a stale
        // value against the freshly loaded one, see a jump, and mistake it for a completed window.
        while (pit_read_status() & PIT_STATUS_NULL_COUNT);

        const uint16_t begin = pit_read_counter();
        const uint64_t tsc_begin = System::rdtsc();

        uint16_t curr = begin;
        bool wrapped = false;
        while (curr > end_count)
        {
            const uint16_t sample = pit_read_counter();

            // The counter only ever counts down within a window, so an increase means it ran
            // through 0 and reloaded - the window is longer than we think and the delta is useless.
            if (sample > curr)
            {
                wrapped = true;
                break;
            }

            curr = sample;
        }

        const uint64_t tsc_end = System::rdtsc();

        if (wrapped)
        {
            printf_warn("PIT: calibrate_tsc: counter wrapped on attempt %i, retrying", attempt);
            continue;
        }

        const uint32_t pit_ticks = begin - curr;
        const double elapsed_us = pit_ticks * 1000000.0 / PIT_FREQUENCY;
        const uint32_t result = (uint32_t)((tsc_end - tsc_begin) / elapsed_us);

        // Sanity-check both ends: any x86 running this kernel clocks between roughly 100MHz and
        // 10GHz. Bounding only the low end would let a systematically inflated result (an
        // undetected mode 3 window, for instance) through unnoticed.
        if (result >= 100 && result <= 10000)
            return result;

        printf_warn("PIT: calibrate_tsc: implausible result %u on attempt %i, retrying", result, attempt);
    }

    printf_warn("PIT: calibrate_tsc: all attempts failed, falling back to a conservative default");
    return 1000;
}

void PIT::init()
{
    tsc_ticks_per_us = calibrate_tsc();
    uint divider = ms_to_pit_divider(CLOCK_TICK_MS);

    // Calibration leaves channel 0 in one-shot mode, so re-arm it as a periodic source for the tick
    pit_set_reload_value(divider, PIT_MODE_SQUARE_WAVE);
}

void sleep_cycles(uint64_t cycles)
{
    uint64_t start = System::rdtsc();
    while (System::rdtsc() - start < cycles);
}

__attribute__((no_instrument_function))
void PIT::sleep(uint ms)
{
    const uint num_ticks_to_wait = (uint)((float)TICKS_PER_SEC * (float)ms / 1000.f);
    if (num_ticks_to_wait == 0)
        sleep_cycles(tsc_ticks_per_us * ms * 1000);
    else
    {
        Scheduler::set_process_asleep(Scheduler::get_running_process(), ms);
        if (Scheduler::preemption_lock)
            Scheduler::critical_section_preempt_exit = true;
        TRIGGER_TIMER_INTERRUPT
    }
}

void PIT::spin_sleep(uint us)
{
    if (us > 100)
        printf_warn("spin_sleep is not recommended for long sleeps, use PIT::sleep instead\n");
    sleep_cycles(tsc_ticks_per_us * us);
}

uint PIT::ms_to_pit_divider(uint ms)
{
    // 1000 / f = ms
    // 1193182 / div = f
    // => div / 1193192 = 1 / f
    // => 1000 * div / 1193182 = 1000 / f = ms
    // div = 1193182ms / 1000
    return (uint)(PIT_FREQUENCY * ms / 1000);
}

uint PIT::ticks = 0;
