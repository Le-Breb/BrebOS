#ifndef INCLUDE_OS_STRING_H
#define INCLUDE_OS_STRING_H

extern "C" unsigned long strlen(char const* str);

extern "C" void memset(void* ptr, int value, unsigned long num);

extern "C" void memcpy(void* dest, const void* src, unsigned long num);

void strcpy(char* dest, const char* src);

void strcat(char* dest, const char* src);

int strcmp(const char* str1, const char* str2);

#endif //INCLUDE_OS_STRING_H
