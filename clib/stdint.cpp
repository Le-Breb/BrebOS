/*
#include "stdint.h"

int64_t __divdi3(int64_t num, int64_t den)
{
	int64_t quot = 0, temp = 0;
	int neg = 0;

	if (num < 0)
	{
		neg ^= 1;
		num = -num;
	}
	if (den < 0)
	{
		neg ^= 1;
		den = -den;
	}

	for (int i = 63; i >= 0; i--)
	{
		temp <<= 1;
		temp |= (num >> i) & 1;
		if (temp >= den)
		{
			temp -= den;
			quot |= (1LL << i);
		}
	}

	return (neg ? -quot : quot);
}

int64_t __moddi3(int64_t num, int64_t den)
{
	int64_t temp = 0;
	int neg = 0;

	if (num < 0)
	{
		neg = 1;
		num = -num;
	}
	if (den < 0)
	{
		den = -den;
	}

	for (int i = 63; i >= 0; i--)
	{
		temp <<= 1;
		temp |= (num >> i) & 1;
		if (temp >= den)
		{
			temp -= den;
		}
	}

	return (neg ? -temp : temp);
}
*/
