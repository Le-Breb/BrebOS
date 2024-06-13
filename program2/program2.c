#include "../klib/syscalls.h"

int p_main()
{
	[[maybe_unused]] unsigned int pid = get_pid();
	[[maybe_unused]] unsigned int c = 1000;
	[[maybe_unused]] unsigned int ctr = 0;

	while (c--)
	{
		printf("Hello from process %u | c = %u\n", pid, ctr++);
	}

	return 0;
}
