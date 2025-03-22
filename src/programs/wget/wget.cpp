#include <kstdio.h>

void wget(const char* uri)
{
    __asm__ volatile("int $0x80" :  : "a"(15), "D"(uri));
}

int main(int argc, char* argv[])
{
    if (argc > 2)
    {
        puts("Usage: wget [uri]\n");
        return 1;
    }
    wget(argc == 1 ? "/" : argv[1]); // Todo: add support for proper options once kernel supports it

    return 0;
}