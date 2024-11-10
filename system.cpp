#include "system.h"
#include "IO.h"
#include "clib/stdio.h"
#include "FAT.h"

extern "C" uint is_running_in_qemu_asm();

[[noreturn]] int System::shutdown()
{
	FAT_drive::shutdown();
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
		{};
	}
}