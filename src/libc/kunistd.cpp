#include "kunistd.h"

pid_t getpid()
{
    pid_t pid;
    __asm__ volatile("int $0x80" : "=a"(pid) : "a"(5));
    return pid;
}

pid_t exec(const char* path, int argc, const char** argv)
{
    pid_t pid;
	__asm__ volatile("int $0x80" : "=a"(pid) : "a"(3), "D"(path), "S"(argc), "d"(argv));

    return pid;
}