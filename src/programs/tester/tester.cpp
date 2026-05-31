#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

extern "C" int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	const char* const prog_name = "42sh";
	const char* const args[] = {prog_name, "-c", "echo execve worked :D", nullptr};
	if (execvp(prog_name, (char**)args) != -1)
	{
		printf("wtf\n");
	}
	else
	{
		printf("failed\n");
	}

	return 0;
}
