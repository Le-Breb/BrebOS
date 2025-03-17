#include "kstdlib.h"

#include <kstring.h>

int atoi(const char *nptr)
{
    int res = 0;
    bool negative = *nptr == '-';
    while (*nptr)
    {
        res *= 10;
        res += *nptr++ - '0';
    }

    return negative ? -res : res;
}

void* malloc(unsigned int n)
{
    void* ptr;
    __asm__ volatile("int $0x80" : "=a"(ptr): "a"(8), "D"(n));
    return ptr;
}

void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    __asm__ volatile("int $0x80" : "=a"(ptr): "a"(17), "D"(nmemb), "S"(size));
    return ptr;
}

void free(void* ptr)
{
    __asm__ volatile("int $0x80" : : "a"(9), "D"(ptr));
}

void* realloc(void *ptr, size_t size)
{
    void* ret_ptr;
    __asm__ volatile("int $0x80" : "=a"(ret_ptr): "a"(18), "D"(ptr), "S"(size));
    return ret_ptr;
}

int rand()
{
    long long tsc;
    asm volatile ("rdtsc" : "=A"(tsc));

    return tsc & (unsigned int)-1;
}

extern "C" [[noreturn]] void exit(int status)
{
    // Todo: call fini functions and __cxa_finalize here and not in start_program.s
    // For now, if this function is called directly from within a program, fini functions and __cxa_finalize aren't run
    __asm__ volatile("int $0x80" : : "a"(1), "D"(status));
    __builtin_unreachable();
}