#include <kstdio.h>
#include <kstdlib.h>
#include <kunistd.h>

extern "C" int main(int argc, char** argv)
{
	printf("argc: %x\n", argc);
	for (int i = 0; i < argc; ++i)
		printf("argv[%d]: %s\n", i, argv[i]);

	// Testing lazy binding - first call triggers libdynkl, next calls have the relocation resolved
	unsigned int pid = getpid();
	unsigned int pid2 = getpid();
	printf("PID:%i - Lazy binding functional: %s\n", pid2, pid == pid2 ? "True" : "False");

	char* a = (char*) malloc(10);
    a[0] = 'h'; // Make sure we have access to the memory returned by malloc
    a[1] = 'e';
    a[2] = 'y';
    a[3] = '\0';
	printf("Userland malloc functional: %s\n", a ? "True" : "False");
	free(a);

	return 0;
}
