#include "../klib/syscalls.h"

int p_main()
{
	char* str = "Hello, World!\n";

	print(str);

	return 0;
}
