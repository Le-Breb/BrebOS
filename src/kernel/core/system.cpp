#include "system.h"

#include "fb.h"
#include "IO.h"
#include "../file_management/FAT.h"
#include "../network/Socket.h"
#include "../processes/scheduler.h"
#include "../utils/profiling.h"

extern "C" uint is_running_in_qemu_asm();

[[noreturn]] int System::shutdown()
{
#ifdef PROFILING
	Profiling::save_data();
#endif
	FAT_drive::shutdown();
	Socket::close_all_connections();
	Scheduler::shutdown();
	uint qemu = is_running_in_qemu_asm();

	if (qemu)
	{
		outb(0x501, 0x31);
		io_wait();
		__builtin_unreachable();
	}
	else
	{
		printf_info("System halted (Hardware shutdown not implemented)");
		while (1)
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