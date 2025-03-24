#include "system.h"

#include "fb.h"
#include "IO.h"
#include <kstdio.h>
#include "../file_management/FAT.h"
#include "../network/Socket.h"
#include "../utils/profiling.h"

extern "C" uint is_running_in_qemu_asm();

[[noreturn]] int System::shutdown()
{
#ifdef PROFILING
	Profiling::save_data();
#endif
	FAT_drive::shutdown();
	Socket::close_all_connections();
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
