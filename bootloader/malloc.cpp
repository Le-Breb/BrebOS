#include "malloc.h"

#define MALLOC_BASE 0x4000

uint sbrk = MALLOC_BASE;

void* malloc(uint n)
{
    void* addr = (void*)sbrk;
    sbrk += n;
    return addr;
}
