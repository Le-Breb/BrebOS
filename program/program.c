#include "../klib/syscalls.h"

int p_main()
{
	// This is INIT program. Thus, it calls syscall 0 to further initialize the kernel
	// This enables preemptive scheduling
	__asm__ volatile("int $0x80" : : "a"(0));
	[[maybe_unused]] unsigned int pid = get_pid();
	[[maybe_unused]] char* _ = "Process 0";
	[[maybe_unused]] unsigned int c = 1000;
	[[maybe_unused]] unsigned int ctr = 1;

	start_process(1);

	while (c--)
	{
		printf("Hello from process %u | c = %u\n", pid, ctr++);
	}

	return 0;
}
