#include "../klib/syscalls.h"

int p_main()
{
	unsigned int pid = get_pid();
	unsigned int c = 10;
	unsigned int ctr = 1;

	start_process(1);

	while (c--)
	{
		printf("Hello from process %u | c = %u\n", pid, ctr++);
	}

	return 0;
}
