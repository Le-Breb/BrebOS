#include <stdio.h>

bool cat(const char* path)
{
    bool success;
    __asm__ volatile("int $0x80" : "=a"(success) : "a"(16), "D"(path));

    return success;
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        puts("Usage: cat <file>\n");
        return 1;
    }

    if (!cat(argv[1]))
    {
        fprintf(stderr, "Error: file '%s' not found\n", argv[1]);
        return 1;
    }
}