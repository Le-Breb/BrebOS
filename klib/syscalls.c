#include "syscalls.h"


__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...)
{
	char* list = (char*) &format + sizeof(format);
	unsigned int i;
	__asm__ volatile("int $0x80" : "=a"(i) : "a"(2), "b"(format), "c"(list));

	return i;
}

_Noreturn void exit()
{
	__asm__ volatile("int $0x80" : : "a"(1));
	__builtin_unreachable();
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

char get_keystroke()
{
	int keystroke;
	__asm__ volatile ("int $0x80" : "=a"(keystroke) : "a"(4));

	return (char) keystroke;
}

_Noreturn void shutdown()
{
	__asm__ volatile("int $0x80" : : "a"(6));

	__builtin_unreachable();
}

void* malloc(unsigned int n)
{
	void* ptr;
	__asm__ volatile("int $0x80" : "=a"(ptr): "a"(8), "b"(n));
	return ptr;
}

void free(void* ptr)
{
	__asm__ volatile("int $0x80" : : "a"(9), "b"(ptr));
}