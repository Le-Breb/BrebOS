#include <stdio.h>

bool touch(const char* path)
{
    bool success;
    __asm__ volatile("int $0x80" : "=a"(success): "a"(11), "D"(path));
    return success;
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
        printf("Failed to create file %s\n", argv[1]);
        return 1;
    }

    return 0;
}