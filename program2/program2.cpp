#include "../klib/syscalls.h"

extern "C" int main(int argc, char** argv)
{
	printf("argc: %x\n", argc);
	for (int i = 0; i < argc; ++i)
		printf("argv[%d]: %s\n", i, argv[i]);

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
