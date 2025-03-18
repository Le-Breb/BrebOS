#include "kctype.h"

bool isdigit(uint c)
{
	return c >= '0' && c <= '9';
}

bool isupper(char c)
{
	return c >= 'A' && c <= 'Z';
}

bool islower(char c)
{
	return c >= 'a' && c <= 'z';
}

bool isalpha(char c)
{
	return isupper(c) || islower(c);
}

bool isalnum(char c)
{
	return isalpha(c) || isdigit(c);
}
