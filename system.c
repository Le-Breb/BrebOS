#include "system.h"
#include "io.h"
#include "lib/stdio.h"

int shutdown()
{
	extern unsigned int is_running_in_qemu();
	unsigned int qemu = is_running_in_qemu();

	if (qemu)
	{
		outb(0x501, 0x31);
		io_wait();
	}
	else
		printf_info("System halted (Hardware shutdown not implemented)");


	return 0;
}