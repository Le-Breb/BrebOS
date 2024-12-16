#include "../kapi/syscalls.h"

extern "C" int main(int argc, char** argv)
{
	printf("argc: %x\n", argc);
	for (int i = 0; i < argc; ++i)
		printf("argv[%d]: %s\n", i, argv[i]);

	// Testing lazy binding - first call triggers libdynkl, next calls have the relocation resolved
	unsigned int pid = get_pid();
	unsigned int pid2 = get_pid();
	printf("PID:%i - Lazy binding functional: %s\n", pid2, pid == pid2 ? "True" : "False");

	char* a = (char*) malloc(10);
	printf("Userland malloc functional: %s\n", a ? "True" : "False");
	free(a);

	return 0;
}
