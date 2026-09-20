#include "std_patch.h"

namespace std
{
    void __throw_bad_alloc()
    {
        irrecoverable_error("bad allocation");
    }

    void __throw_bad_array_new_length()
    {
        irrecoverable_error("bad array length");
    }
}

extern "C" [[noreturn]] void abort()
{
    irrecoverable_error("abort called from %p", __builtin_return_address(0));
}
