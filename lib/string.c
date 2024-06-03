#include "string.h"

unsigned long strlen(const char* str)
{
	int len = 0;
	while (str[len])
		len++;
	return len;
}

void memset(void* ptr, unsigned char value, unsigned long num)
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
