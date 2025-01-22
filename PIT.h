#ifndef CUSTOM_OS_PIT_H
#define CUSTOM_OS_PIT_H

#include "libc/stddef.h"

#define CLOCK_TICK_MS 10
#define TICKS_PER_SEC (1000 / CLOCK_TICK_MS)

class PIT
{
	/**
	 * Computes the divider to send to PIT to make it send interrupts every ms milliseconds
	 * @param ms desired PIT delay
	 * @return PIT divider
	 */
	static uint ms_to_pit_divider(uint ms);

public:
	static uint ticks; // PIT tick since it's been initialized

	static uint get_tick();

	/**
	 * Set up the PIT
	 */
	static void init();

	static void sleep(uint ms);
};


#endif //CUSTOM_OS_PIT_H
