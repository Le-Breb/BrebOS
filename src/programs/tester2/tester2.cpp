#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ksyscalls.h>

extern "C" int main(int argc, char** argv)
{
	pid_t child = fork();
	if (child == 0)
	{
		printf("Child process PID: %u\n", getpid());
		return 0;
	}
	if (child > 0)
	{
		[[maybe_unused]] int wstatus;
		waitpid(child, &wstatus, 0);
		printf("Finished waiting for child PID: %u\n", child);
	}
	else
	{
		printf("Fork failed\n");
	}
	printf("argc: %x\n", argc);
	for (int i = 0; i < argc; ++i)
		printf("argv[%d]: %s\n", i, argv[i]);

	// Testing lazy binding - first call triggers libdynkl, next calls have the relocation resolved
	unsigned int w1, w2, h1, h2;
	get_screen_dimensions(&w1, &h1);
	get_screen_dimensions(&w2, &h2);
	printf("PID:%i - Lazy binding functional: %s\n", getpid(), w1 == w2 && h1 == h2 ? "True" : "False");

	const size_t N = 4096000;
	char* a = (char*) calloc(N, 1);
	   a[0] = 'h'; // Make sure we have access to the memory returned by malloc
	   a[1] = 'e';
	   a[2] = 'y';
	   a[3] = '\0';
	//for (size_t i = 0; i < N; i++)
		//a[i] = 'h';
	printf("Userland malloc functional: %s\n", a ? "True" : "False");
	free(a);

	// for (uint i = 0; i < 20000; i++)
	// 	printf("%u\n", i);

	return 0;
}
