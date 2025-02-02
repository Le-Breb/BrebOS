#include "ksyscalls.h"

extern "C" [[noreturn]] void exit()
{
	__asm__ volatile("int $0x80" : : "a"(1));
	__builtin_unreachable();
}

void exec(const char* path, int argc, const char** argv)
{
	__asm__ volatile("int $0x80" : : "a"(3), "D"(path), "S"(argc), "d"(argv));
}

void putchar(char c)
{
	const char b[] = { c, 0 };
   	puts(b);
}

void puts(const char* str)
{
	__asm__ volatile("int $0x80" : : "a"(2), "S"(str));
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

[[noreturn]] void shutdown()
{
	__asm__ volatile("int $0x80" : : "a"(6));

	__builtin_unreachable();
}

void* malloc(unsigned int n)
{
	void* ptr;
	__asm__ volatile("int $0x80" : "=a"(ptr): "a"(8), "D"(n));
	return ptr;
}

void free(void* ptr)
{
	__asm__ volatile("int $0x80" : : "a"(9), "D"(ptr));
}

bool mkdir(const char* path)
{
	bool success;
	__asm__ volatile("int $0x80" : "=a"(success): "a"(10), "D"(path));
	return success;
}

bool touch(const char* path)
{
	bool success;
	__asm__ volatile("int $0x80" : "=a"(success): "a"(11), "D"(path));
	return success;
}

bool ls(const char* path)
{
	bool success;
	__asm__ volatile("int $0x80" : "=a"(success): "a"(12), "D"(path));
	return success;
}