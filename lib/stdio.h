#ifndef INCLUDE_STDIO_H
#define INCLUDE_STDIO_H

// Define va_start macro
#define va_start(ap, last_arg) (ap = (va_list)&last_arg + sizeof(last_arg))

// Define va_arg macro
#define va_arg(ap, type) (*(type*)((ap += sizeof(type)) - sizeof(type)))

// Define va_end macro (no-op in this case)
#define va_end(ap) ((void)0)

typedef char* va_list;

__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...);

__attribute__ ((format (printf, 1, 2))) int printf_error(const char* format, ...);

__attribute__ ((format (printf, 1, 2))) int printf_info(const char* format, ...);

#endif //INCLUDE_STDIO_H
