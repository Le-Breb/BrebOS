#ifndef INCLUDE_OS_STRING_H
#define INCLUDE_OS_STRING_H

#include <kstddef.h>

extern "C" unsigned long strlen(char const* str);

extern "C" void* memset(void* ptr, int value, unsigned long num);

extern "C" void* memcpy(void* dest, const void* src, unsigned long num);

char* strcpy(char* dest, const char* src);

char* strcat(char* dest, const char* src);

char* strncat(char* dest, const char* src, size_t ssize);

int strcmp(const char* str1, const char* str2);

char* strtok_r(char* str, const char* delim, char** saveptr);

int memcmp(const void* s1, const void* s2, size_t n);

void to_lower_in_place(char* str);

size_t strspn(const char *s, const char *accept);

size_t strcspn(const char *s, const char *reject);

char* strdup(const char *s);

char* strndup(const char* s, size_t n);

void *memmove(void *dest, const void *src, size_t n);

char* strchr(const char *s, int c);

char* strncpy(char *dst, const char* src, size_t dsize);

char* stpncpy(char * dst, const char * src, size_t dsize);

void* mempcpy(void* dest, const void* src, size_t n);

size_t strnlen(const char* s, size_t maxlen);

char* strstr(const char *haystack, const char *needle);
#endif //INCLUDE_OS_STRING_H
