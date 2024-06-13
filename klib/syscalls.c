#include "syscalls.h"

void print(char* str)
{
	__asm__ volatile("int $0x80" : : "a"(2), "b"(str));
}

__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...)
{
	char* list = (char*) &format + sizeof(format);
	unsigned int i;
	__asm__ volatile("int $0x80" : "=a"(i) : "a"(2), "b"(format), "c"(list));

	return i;
}

void exit()
{
	__asm__ volatile("int $0x80" : : "a"(1));
}

void start_process(unsigned int module_id)
{
	__asm__ volatile("int $0x80" : : "a"(3), "b"(module_id));
}

unsigned int get_pid()
{
	unsigned int pid;
	__asm__ volatile("int $0x80" : "=a"(pid) : "a"(5));
	return pid;
}
