#ifndef INCLUDE_OS_STRING_H
#define INCLUDE_OS_STRING_H

unsigned long strlen(const char* str);

void memset(void* ptr, unsigned char value, unsigned long num);

void memcpy(void* dest, const void* src, unsigned long num);

void strcpy(char* dest, const char* src);

void strcat(char* dest, const char* src);

int strcmp(const char* str1, const char* str2);

#endif //INCLUDE_OS_STRING_H
