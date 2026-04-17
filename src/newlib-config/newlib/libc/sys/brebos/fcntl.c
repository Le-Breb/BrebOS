#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>

int fcntl(int fd, int cmd, ...)
{
    int ret;
    va_list args;

    va_start(args, cmd);

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(38), "D"(fd), "S"(cmd), "d"(args));

    va_end(args);

    if (ret >= 0)
        return ret;

    errno = -ret;

    return -1;
}