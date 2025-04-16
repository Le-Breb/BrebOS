#include "kwait.h"

pid_t waitpid(pid_t pid, int* wstatus)
{
    pid_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14), "D"(pid), "S"(wstatus));

    return ret;
}