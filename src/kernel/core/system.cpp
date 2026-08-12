#include "system.h"

#include "fb.h"
#include "IO.h"
#include "../file_management/FAT.h"
#include "../network/Socket.h"
#include "../processes/scheduler.h"
#include "../utils/profiling.h"

extern "C" uint is_running_in_qemu_asm();

bool System::irrecoverable_error_happened = false;

[[noreturn]] int System::shutdown()
{
#ifdef PROFILING
	Profiling::save_data();
#endif
	VFS::shutdown();
	FAT_drive::shutdown();
	Socket::close_all_connections();
	Scheduler::shutdown();

	if (is_running_in_qemu_asm())
	{
		outb(0x501, 0x31);
		io_wait();
		__builtin_unreachable();
	}
	else
	{
		FB::draw_fullscreen_message("It's now safe to turn off your computer");
		while (true)
		{
		};
	}
}


[[nodiscard]]
uint64_t System::rdtsc()
{
	uint32_t hi, lo;
	__asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
	return ((uint64_t)hi << 32) | lo;
}