#include <kstdio.h>

bool ls(const char* path)
{
    bool success;
    __asm__ volatile("int $0x80" : "=a"(success): "a"(12), "D"(path));
    return success;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "ls: Usage: ls <path>\n");
        return 1;
    }

    if (!ls(argv[1]))
    {
        fprintf(stderr, "ls: error displaying content of '%s'\n", argv[1]);
        return 1;
    }

    return 0;
}