#include  <sys/wait.h>
#include <sys/errno.h>


pid_t waitpid (pid_t  pid, int* wstatus, int options)
{
    if (options)
    {
        errno = EINVAL;
        return -1;
    }
    
    if (pid <= 0)
    {
        errno = ECHILD;
        return -1;
    }

    pid_t ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(14), "D"(pid), "S"(wstatus));

    return ret;
}
