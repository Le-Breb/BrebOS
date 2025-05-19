#include <stdlib.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    int N = 4096000;
    char* a = (char*)malloc(N);
    
    for (int i = 0; i < N; i++)
        a[i] = 'h';

    free(a);

    return 0;
}
