#include <unistd.h>
#include <stdlib.h>
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

char *getcwd(char* buf, size_t size)
{
    const size_t MAX_PATH_SIZE = 200;
    // Man page indicates that getcwd allocates a buffer if size == 0 and buf == nullptr
    // Allocation must be done in userland. Indeed, it could be done with process allocation from kernel land, but then
    // newlib would not have handled the allocation, and would then fail to free it later on.
    if (size == 0 && !buf)
    {
        size = MAX_PATH_SIZE;
        buf = (char*)malloc(size);
        if (!buf)
        {
            errno = ENOMEM;
            return nullptr;
        }
    }

    char* ret;
    int err;
    __asm__ volatile("int $0x80" : "=a"(ret), "=D"(err) : "a"(43), "D"(buf), "S"(size));

    if (ret == nullptr)
    {
        errno = err;
        return nullptr;
    }
    return ret;
}

int chdir(const char *path)
{
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret): "a"(44), "D"(path));

    if (ret < 0)
    {
        errno = -ret;
        return -1;
    }

    return ret;
}