#include "syscalls.h"

void print(char* str)
{
	__asm__ volatile("int $0x80" : : "a"(2), "b"(str));
}

void exit()
{
	__asm__ volatile("int $0x80" : : "a"(1));
}

void start_process(unsigned int module_id)
{
	__asm__ volatile("int $0x80" : : "a"(3), "b"(module_id));
}

void pause()
{
	__asm__ volatile("int $0x80" : : "a"(4));
}

unsigned int get_pid()
{
	unsigned int pid;
	__asm__ volatile("int $0x80" : "=a"(pid) : "a"(5));
	return pid;
}
