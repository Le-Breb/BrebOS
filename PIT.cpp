#include "PIT.h"
#include "IO.h"

uint PIT::get_tick()
{
	return ticks;
}

void PIT::init()
{
	uint divider = ms_to_pit_divider(CLOCK_TICK_MS);

	outb(0x43, 0b00110110);

	outb(0x40, divider & 0xFF); // Send low byte of divider
	outb(0x40, (divider >> 8) & 0xFF); // Send high byte of divider
}

void PIT::sleep(uint ms)
{
	uint tick = ticks;
	uint num_ticks_to_wait = TICKS_PER_SEC * ms / 1000;
	// If wait duration is less than 1 tick, wait 1 tick anyway // Todo: use high precision timers to handle it
	if (!num_ticks_to_wait)
		num_ticks_to_wait = 1;
	// ReSharper disable CppDFALoopConditionNotUpdated
	while (ticks - tick < num_ticks_to_wait)
		// ReSharper restore CppDFALoopConditionNotUpdated
		__asm__ __volatile__("hlt"); // Todo: Use something smarter, like schedule_timeout when it will be implemented
}

uint PIT::ms_to_pit_divider(uint ms)
{
	// 1000 / f = ms
	// 1193182 / div = f
	// => div / 1193192 = 1 / f
	// => 1000 * div / 1193182 = 1000 / f = ms
	// div = 1193182ms / 1000
	return 1193182 * ms / 1000;
}

uint PIT::ticks = 0;
