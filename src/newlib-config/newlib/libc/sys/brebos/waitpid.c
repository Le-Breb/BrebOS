#include <sys/wait.h>
#include <sys/errno.h>

pid_t waitpid (pid_t  pid, int* wstatus, int options)
{
    if (options)
    {
        errno = EINVAL;
        return -1;
    }
   
    // < -1 not handled:
    if (pid < -1)
    {
        errno = ECHILD;
        return -1;
    }

    pid_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14), "D"(pid), "S"(wstatus));

    if (ret >= 0)
        return ret;

    errno = -ret;

    return -1;
}
