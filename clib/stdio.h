#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

// Define va_start macro
#include "stdint.h"

#define va_start(ap, last_arg) (ap = (va_list)&last_arg + sizeof(last_arg))

// Define va_arg macro
#define va_arg(ap, type) (*(type*)((ap += sizeof(type)) - sizeof(type)))

// Define va_end macro (no-op in this case)
#define va_end(ap) ((void)0)

char* __int_str(intmax_t i, char b[], int base, unsigned int plusSignIfNeeded, unsigned int spaceSignIfNeeded,
				int paddingNo, unsigned int justify, unsigned int zeroPad);

typedef char* va_list;

__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...);

__attribute__ ((format (printf, 1, 2))) int printf_error(const char* format, ...);

__attribute__ ((format (printf, 1, 2))) int printf_info(const char* format, ...);

/**
 * Printf to be called from printf syscall. \n
 * The difference is that format specifiers are not on this function's stack but rather on the user process stack,
 * which this function handles appropriately.
 * @param format format string to print
 * @param args pointer to format arguments
 * @return Number of characters written
 */
int printf_syscall(const char* format, char* args);

#endif //INCLUDE_STDIO_H
