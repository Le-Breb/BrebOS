#ifndef CUSTOM_OS_PIT_H
#define CUSTOM_OS_PIT_H

#include <kstddef.h>

#include <stdint.h>

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

// Operating modes, already shifted into the position the command byte expects (bits 3-1).
// Mode 0 decrements the counter once per input clock; mode 3 decrements it by two to build a
// square wave, so a given reload value covers half as much wall time there as it does in mode 0.
#define PIT_MODE_ONE_SHOT       (0 << 1) // Interrupt on terminal count, counts down once
#define PIT_MODE_SQUARE_WAVE    (3 << 1) // Periodic, used for the kernel tick

// Status byte bit (see pit_read_status): reload value written but not yet loaded into the counter
#define PIT_STATUS_NULL_COUNT   (1 << 6)

// PIT input clock frequency, in Hz
#define PIT_FREQUENCY 1193182.0

#define CLOCK_TICK_MS 10
#define TICKS_PER_SEC (1000 / CLOCK_TICK_MS)

class PIT
{
	static uint32_t tsc_ticks_per_us;

	/**
	 * Computes the divider to send to PIT to make it send interrupts every ms milliseconds
	 * @param ms desired PIT delay
	 * @return PIT divider
	 */
	static uint ms_to_pit_divider(uint ms);

	/**
	 * Use the PIT to calibrate the TSC
	 * @return TSC ticks per microsecond
	 */
	static uint32_t calibrate_tsc();

public:
	static uint ticks; // PIT tick since it's been initialized

	static uint get_tick();

	/**
	 * Set up the PIT
	 */
	static void init();

	/**
	 * Sleeps for ms milliseconds
	 * @parameter ms number of milliseconds to wait
	 */
	__attribute__((no_instrument_function))
	static void sleep(uint ms);

	static void spin_sleep(uint us);
};


#endif //CUSTOM_OS_PIT_H
