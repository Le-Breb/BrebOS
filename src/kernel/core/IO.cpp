#include "IO.h"

__attribute__((optimize("O0"))) // Compiler optimizations may mess up with the logic here
void IO::insl(uint port, void* buffer, uint quads)
{
	uint* buf = static_cast<uint*>(buffer);

	// Save ES and set it to DS
	asm volatile(
		"pushw %%es\n\t"       // Save the current ES value
		"movw %%ds, %%ax\n\t"  // Copy DS to AX
		"movw %%ax, %%es\n\t"  // Set ES to DS
		:
		:
		: "ax", "memory"       // Clobber AX, and memory may be modified
	);

	// Perform the I/O operation
	for (size_t i = 0; i < quads; ++i) {
		asm volatile(
			"inl %[port], %[value]"  // Read from port and store into buffer
			: [value] "=a"(buf[i])   // Output: store result in buf[i]
			: [port] "Nd"(port)      // Input: port number
		);
	}

	// Restore ES
	asm volatile(
		"popw %%es\n\t"       // Restore the original ES value
		:
		:
		: "memory"            // Memory may be modified
	);
}
