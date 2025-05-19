#include <sys/stat.h>
#include <stdio.h>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        puts("Usage: mkdir <path>\n");
        return 1;
    }

    if (mkdir(argv[1], 0777) == -1)
    {
        printf("Failed to create directory %s\n", argv[1]);
        return 1;
    }

    return 0;
}