#include "string.h"

unsigned long strlen2(const char* str)
{
	int len = 0;
	while (str[len])
		len++;
	return len;
}