#include "system.h"
#include "io.h"
#include "clib/stdio.h"

extern unsigned int is_running_in_qemu_asm();

int shutdown()
{
	unsigned int qemu = is_running_in_qemu_asm();

	if (qemu)
	{
		outb(0x501, 0x31);
		io_wait();
	}
	else
		printf_info("System halted (Hardware shutdown not implemented)");


	return 0;
}