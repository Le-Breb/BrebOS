#pragma once

#include <stddef.h>
typedef unsigned int uint;

/** Tries to allocate a contiguous block of memory
 *
 * @param n Size of the block in bytes
 * @return Address of the beginning of allocated block if allocation was successful, NULL otherwise
 */
extern "C" void* malloc(uint n);

extern "C" void* realloc(void* ptr, size_t size);

extern "C" void* calloc(size_t nmemb, size_t size);

/** Frees some memory
 *
 * @param ptr Pointer to the memory block to free
 */
extern "C" void free(void* ptr);