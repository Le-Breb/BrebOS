#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

bool touch(const char* path)
{
    int fd = open(path, O_RDWR | O_CREAT, 0777);
    if (fd == -1)
        return false;
    close(fd);
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        puts("Usage: touch <path>\n");
        return 1;
    }

    if (!touch(argv[1]))
    {
        perror("Failed to create file");
        return 1;
    }

    return 0;
}