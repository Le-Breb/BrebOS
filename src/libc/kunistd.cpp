#include "kunistd.h"

pid_t getpid()
{
    pid_t pid;
    __asm__ volatile("int $0x80" : "=a"(pid) : "a"(5));
    return pid;
}

void exec(const char* path, int argc, const char** argv)
{
	__asm__ volatile("int $0x80" : : "a"(3), "D"(path), "S"(argc), "d"(argv));
}