#ifndef CUSTOM_OS_PIT_H
#define CUSTOM_OS_PIT_H

#include <kstddef.h>

#include "kstdint.h"

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

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

	__attribute__((no_instrument_function))
	static void sleep(uint ms);
};


#endif //CUSTOM_OS_PIT_H
