#ifndef INCLUDE_OS_STRING_H
#define INCLUDE_OS_STRING_H

#include <kstddef.h>

extern "C" unsigned long strlen(char const* str);

extern "C" void memset(void* ptr, int value, unsigned long num);

extern "C" void memcpy(void* dest, const void* src, unsigned long num);

void strcpy(char* dest, const char* src);

void strcat(char* dest, const char* src);

int strcmp(const char* str1, const char* str2);

char* strtok_r(char* str, const char* delim, char** saveptr);

int memcmp(const void* s1, const void* s2, size_t n);

void to_lower_in_place(char* str);

#endif //INCLUDE_OS_STRING_H
