#ifndef CUSTOM_OS_PIT_H
#define CUSTOM_OS_PIT_H

#include "clib/stddef.h"

#define CLOCK_TICK_MS 10
#define TICKS_PER_SEC (1000 / CLOCK_TICK_MS)

class PIT
{
	/**
	 * Computes the divider to send to PIT to make it send interrupts every ms milliseconds
	 * @param ms desired PIT delay
	 * @return PIT divider
	 */
	static unsigned int ms_to_pit_divider(unsigned int ms);

public:
	static uint ticks; // PIT tick since it's been initialized

	static unsigned int get_tick();

	/**
	 * Set up the PIT
	 */
	static void init();
};


#endif //CUSTOM_OS_PIT_H
