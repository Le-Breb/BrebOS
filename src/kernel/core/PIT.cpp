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

void pit_set_reload_value(uint16_t value)
{
    // Command byte: channel 0, access lobyte/hibyte, mode 0 (one-shot), binary mode
    outb(PIT_COMMAND_PORT, 0b00110110);

    outb(PIT_CHANNEL0_PORT, value & 0xFF); // Send low byte of divider
    outb(PIT_CHANNEL0_PORT, (value >> 8) & 0xFF); // Send high byte of divider
}

uint32_t PIT::calibrate_tsc()
{
    pit_set_reload_value(0xFFFF); // Set PIT to max count

    uint64_t tsc_start = System::rdtsc();

    // Wait for the PIT counter to reach 0
    uint16_t prev = pit_read_counter();
    while (true)
    {
        uint16_t curr = pit_read_counter();
        if (curr > prev) break; // Wrapped around
        prev = curr;
    }

    uint64_t tsc_end = System::rdtsc();

    uint64_t elapsed = tsc_end - tsc_start;

    // PIT counts at 1.193182 MHz (ticks per second)
    // 0xFFFF is 65535 ticks, so time elapsed is:
    double pit_time_us = 65535.0 * 1000000.0 / 1193182.0; // in microseconds

    // TSC ticks per microsecond:
    return (uint32_t)(elapsed / pit_time_us);
}

void PIT::init()
{
    tsc_ticks_per_us = calibrate_tsc();
    uint divider = ms_to_pit_divider(CLOCK_TICK_MS);

    pit_set_reload_value(divider);
}

void sleep_cycles(uint64_t cycles)
{
    uint64_t start = System::rdtsc();
    while (System::rdtsc() - start < cycles);
}

__attribute__((no_instrument_function))
void PIT::sleep(uint ms)
{
    uint num_ticks_to_wait = (uint)((float)TICKS_PER_SEC * (float)ms / 1000.f);
    if (num_ticks_to_wait == 0)
        sleep_cycles(tsc_ticks_per_us * ms * 1000);
    else
    {
        Scheduler::set_process_asleep(Scheduler::get_running_process(), ms);
        TRIGGER_TIMER_INTERRUPT
    }
}

uint PIT::ms_to_pit_divider(uint ms)
{
    // 1000 / f = ms
    // 1193182 / div = f
    // => div / 1193192 = 1 / f
    // => 1000 * div / 1193182 = 1000 / f = ms
    // div = 1193182ms / 1000
    return (uint)(1193182.0 * ms / 1000);
}

uint PIT::ticks = 0;
