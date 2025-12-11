#include "malloc.h"

uint sbrk = MALLOC_BASE;

// Super simple memory allocator: consider that memory above MALLOC_BASE is free,
// and contiguously allocates memory blocks starting from there.
// Do not keep track of allocated memory, since once stage2 exits we do not need
// it anymore, and kernel will consider it free (ie freeing the memory is done
// simply by jumping in the kernel)
void* malloc(uint n)
{
    void* addr = (void*)sbrk;
    sbrk += n;
    return addr;
}
