#include <unistd.h>
#include <errno.h>

int dup2(int oldfd, int newfd)
{
    int ret;

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(39), "D"(oldfd), "S"(newfd));

    if (ret >= 0)
        return ret;

    errno = -ret;

    return -1;
}

int dup(int fd)
{
    int ret;

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(40), "D"(fd));

    if (ret >= 0)
        return ret;

    errno = -ret;

    return -1;
}

int pipe(int pipefd[2])
{
    int ret;

    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(41), "D"(pipefd));

    if (ret >= 0)
        return ret;

    errno = -ret;

    return -1;
}

int execvp(const char *file, char *const argv[])
{
    int argc = 0;
    for (char* const* argv2 = argv; *argv2; argv2++, argc++){};

    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(42), "D"(file), "S"(argc), "d"(argv));

    // If this point  is reached, then execve failed
    errno = ENOMEM;
    return -1;
}