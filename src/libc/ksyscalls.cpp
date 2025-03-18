#include "ksyscalls.h"

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

pid_t getpid()
{
	pid_t pid;
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

void clear_screen()
{
	__asm__ volatile("int $0x80" :  : "a"(13));
}

void dns(const char* domain)
{
	__asm__ volatile("int $0x80" :  : "a"(14), "D"(domain));
}

void wget(const char* uri)
{
	__asm__ volatile("int $0x80" :  : "a"(15), "D"(uri));
}

void cat(const char* path)
{
	__asm__ volatile("int $0x80" :  : "a"(16), "D"(path));
}