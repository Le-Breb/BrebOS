#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

// Define va_start macro
#include "kstdint.h"
#include "kstddef.h"

#define stdin 0
#define stdout 1
#define stderr 2

#define va_start(ap, last_arg) (ap = (va_list)&last_arg + sizeof(last_arg))

// Define va_arg macro
#define va_arg(ap, type) (*(type*)((ap += sizeof(type)) - sizeof(type)))

// Define va_end macro (no-op in this case)
#define va_end(ap) ((void)0)

#define FILE int
#define SEEK_CUR 1

char* k__int_str(intmax_t i, char b[], int base, uint plusSignIfNeeded, uint spaceSignIfNeeded,
				int paddingNo, uint justify, uint zeroPad);

typedef char* va_list;

__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...);

__attribute__((format(printf, 2, 3))) int sprintf(char* str, const char *format, ...);

__attribute__((format(printf, 2, 3))) int fprintf(int stream, const char* format, ...);

void flush();

void puts(const char* str);

void putchar(char c);

int fseek(FILE *stream, long offset, int whence);

int fclose(FILE *stream);

/*
__attribute__ ((format (printf, 1, 2))) int printf_error(const char* format, ...);

__attribute__ ((format (printf, 1, 2))) int printf_info(const char* format, ...);*/

#endif //INCLUDE_STDIO_H
