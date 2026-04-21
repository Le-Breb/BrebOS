#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    void* buf = malloc(10000);
    int r = 0;
    int tot = 0;
    while ((r = read(0, (char*)buf + 10 * tot / 10, 10)) > 0)
    {tot += r;}

    if (r == -1)
    {
        fprintf(stderr, "Read error");
        exit(1);
    }

    for (int i = 0; i < tot; i++)
        putchar(*((char*)buf + i));

    free(buf);

    return 0;
}
