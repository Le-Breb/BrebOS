#include "../klib/syscalls.h"

int p_main()
{
	// Testing lazy binding - first call triggers libdynkl, next calls have the relocation resolved
	unsigned int pid = get_pid();
	printf("test:%i", pid);
	unsigned int pid2 = get_pid();
	printf("test:%i", pid2);

	return 0;
}
