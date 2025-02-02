#include <kstdio.h>
#include <ksyscalls.h>

extern "C" int main(int argc, char** argv)
{
	printf("argc: %x\n", argc);
	for (int i = 0; i < argc; ++i)
		printf("argv[%d]: %s\n", i, argv[i]);

	printf("I am a program\n");

	return 0;
}
