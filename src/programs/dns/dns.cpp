#include <kstdio.h>

void dns(const char* domain)
{
    __asm__ volatile("int $0x80" :  : "a"(14), "D"(domain));
}

int main(int argc, char* argv[])
{
    if (argc > 2)
    {
        puts("Usage: dns [domain]\n");
        return 1;
    }
    dns(argc == 1 ? "www.example.com" : argv[1]);

    return 0;
}