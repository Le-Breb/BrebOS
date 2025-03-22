#include <ksyscalls.h>
#include <kstdio.h>

bool mkdir(const char* path)
{
    bool success;
    __asm__ volatile("int $0x80" : "=a"(success): "a"(10), "D"(path));
    return success;
}

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        puts("Usage: mkdir <path>\n");
        return 1;
    }

    if (!mkdir(argv[1]))
    {
        printf("Failed to create directory %s\n", argv[1]);
        return 1;
    }

    return 0;
}