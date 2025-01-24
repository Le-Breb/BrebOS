#include "IO.h"

void IO::insl(uint port, void* buffer, uint quads)
{
	uint* buf = (uint*)(buffer);

	for (size_t i = 0; i < quads; ++i) {
		asm volatile (
				"inl %[port], %[value]"
				: [value] "=a" (buf[i])  // Output: value is stored in buf[i]
		: [port] "Nd" (port)     // Input: port number
		);
	}
}