#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

bool ls(const char* path)
{
    bool success;
    __asm__ volatile("int $0x80" : "=a"(success): "a"(12), "D"(path));
    return success;
}

int main(int argc, char** argv)
{
    if (argc != 2 && argc != 1)
    {
        fprintf(stderr, "ls: Usage: ls [path]\n");
        return 1;
    }

    char* path = argc == 1 ? getcwd(nullptr, 0) : argv[1];

    if (!ls(path))
    {
        fprintf(stderr, "ls: error displaying content of '%s'\n", argv[1]);
        return 1;
    }

    if (argc == 1)
        free(path);

    return 0;
}