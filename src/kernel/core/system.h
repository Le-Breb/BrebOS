#ifndef INCLUDE_SYSTEM_H
#define INCLUDE_SYSTEM_H
#include <stdint.h>

class System
{
public:
	[[noreturn]]
	static int shutdown();

	[[nodiscard]]
	static uint64_t rdtsc();

	static bool irrecoverable_error_happened;
};

#endif //INCLUDE_SYSTEM_H
