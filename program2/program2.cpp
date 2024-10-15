#include "../klib/syscalls.h"

int main()
{
	// Testing lazy binding - first call triggers libdynkl, next calls have the relocation resolved
	unsigned int pid = get_pid();
	printf("test:%i", pid);
	unsigned int pid2 = get_pid();
	printf("test:%i", pid2);

	char* a = (char*) malloc(10);
	printf("%p\n", a);
	free(a);

	return 0;
}
