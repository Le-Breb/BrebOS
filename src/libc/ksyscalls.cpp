#include "ksyscalls.h"

char get_keystroke()
{
	int keystroke;
	__asm__ volatile ("int $0x80" : "=a"(keystroke) : "a"(4));

	return (char) keystroke;
}