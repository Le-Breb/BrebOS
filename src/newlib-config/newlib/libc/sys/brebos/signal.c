#include <signal.h>

_sig_func_ptr signal(int signal, _sig_func_ptr handler)
{
    _sig_func_ptr ret;

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(36), "D"(signal), "S"(handler));

    return ret;
}
