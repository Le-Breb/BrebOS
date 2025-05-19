#include <sys/stat.h>
#include <sys/errno.h>

#undef errno
extern int errno;

int	mkdir(const char *pathname, [[maybe_unused]] mode_t mode)
{
    if (mode != 0777)
    {
        errno = EIO;
        return -1;
    }

    int state;
    __asm__ volatile("int $0x80" : "=a"(state) : "a"(10), "D"(pathname));


    if (!state)
        errno = EIO;

    return state ? 0 : -1;
}

