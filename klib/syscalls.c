#include "syscalls.h"

void print(char* str)
{
	__asm__ volatile("int $0x80" : : "a"(2), "b"(str));
}

void exit()
{
	__asm__ volatile("int $0x80" : : "a"(1));
}
