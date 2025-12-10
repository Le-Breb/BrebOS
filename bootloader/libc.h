#ifndef BREBOS_LIBC_HPP
#define BREBOS_LIBC_HPP

unsigned long strlen(char const* str);

void* memset(void* ptr, int value, unsigned long num);

char* strcpy(char* dest, const char* src);

bool strcmp(const char* str1, const char* str2);

void* memcpy(void* dest, const void* src, unsigned long num);
#endif //BREBOS_LIBC_HPP