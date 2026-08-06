#include "USB.h"

__attribute__ ((format (printf, 1, 2))) extern int printf_warn(const char* format, ...);

USB* USB::get_instance()
{
    static USB* instance = nullptr;

    if (instance)
        return instance;
    if (!xHCI::get_instance())
    {
        printf_warn("Cannot get USB instance as no xCHI controller is available");
        return nullptr;
    }
    return new USB();
}
