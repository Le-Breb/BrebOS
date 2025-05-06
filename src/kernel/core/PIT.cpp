#include "PIT.h"
#include "IO.h"
#include "../processes/scheduler.h"

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
	Scheduler::set_process_asleep(Scheduler::get_running_process(), ms);
	TRIGGER_TIMER_INTERRUPT
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
