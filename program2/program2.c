#include "../klib/syscalls.h"

int p_main()
{
	char str[] = "Hello from process x\n";
	str[19] = '0' + (char) get_pid();

	for (int i = 0; i < 5; ++i)
	{
		print(str);
		pause();
	}

	return 0;
}
