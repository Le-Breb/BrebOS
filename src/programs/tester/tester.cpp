#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

extern "C" int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	const char* const prog_name = "cls";
	const char* const args[] = {prog_name, nullptr};
	if (execve("cls", (char**)args, nullptr) != -1)
	{
		printf("wtf\n");
	}
	else
	{
		printf("failed\n");
	}

	return 0;
}
