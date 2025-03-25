#include "kerr.h"

#include <kstdio.h>
#include <kstdlib.h>

[[noreturn]] void errx(int eval, const char *fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    printf(fmt, list);
    va_end (list);
    flush();

    exit(eval);
}
