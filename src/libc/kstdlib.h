#ifndef KSTDLIB_H
#define KSTDLIB_H

#include "kstddef.h"

int atoi(const char *nptr);

int rand();

/**
 * Tries to allocate n contiguous bytes of memory on the heap
 * @param n number of bytes to allocate
 * @return pointer to beginning of allocated memory block or NULL if allocation failed
 */
void* malloc(unsigned int n);

void* calloc(size_t nmemb, size_t size);

/**
 * Frees a memory block
 * @param ptr pointer to the beginning of the block
 */
void free(void* ptr);

void* realloc(void *ptr, size_t size);

/** Terminates the program
 * @note This function is marked as extern "C" to disable CPP name mangling, thus making the function callable from
 * assembly
 * */
extern "C" [[noreturn]] void exit(int status);

#endif //KSTDLIB_H
