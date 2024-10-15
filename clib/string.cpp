#include "string.h"

extern "C" unsigned long strlen(char const* str)
{
	int len = 0;
	while (str[len])
		len++;
	return len;
}

void memset(void* ptr, int value, unsigned long num)
{
	unsigned char* p = (unsigned char*) ptr;
	for (unsigned long i = 0; i < num; i++)
		p[i] = value;
}

void strcpy(char* dest, const char* src)
{
	while (*src)
		*dest++ = *src++;
	*dest = '\0';
}

void strcat(char* dest, const char* src)
{
	while (*dest)
		dest++;
	while (*src)
		*dest++ = *src++;
	*dest = '\0';
}

extern "C" void memcpy(void* dest, const void* src, unsigned long num)
{
	unsigned char* d = (unsigned char*) dest;
	const unsigned char* s = (const unsigned char*) src;
	for (unsigned long i = 0; i < num; i++)
		d[i] = s[i];
}

int strcmp(const char* str1, const char* str2)
{
	for (; *str1 && *str2 && *str1 == *str2; str1++, str2++);

	return *str1 - *str2;
}
