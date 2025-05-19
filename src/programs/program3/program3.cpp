#include <stdio.h>
#include "program3.h"

extern "C" int main(int argc, char** argv)
{
	printf("argc: %x\n", argc);
	for (int i = 0; i < argc; ++i)
		printf("argv[%d]: %s\n", i, argv[i]);

    struct test t;
	printf("I am a program which can use a .h. I can prove it by showing this value:%d\n", t.a);

	return 0;
}
