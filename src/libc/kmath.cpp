// When FPU will be enabled, it could be great to use https://git.musl-libc.org/cgit/musl/tree/src/math/

#include "kmath.h"
double floor(double x)
{
    int xi = (int)x;
    if (x < 0 && x != (double)xi) {
        return xi - 1;
    }
    return xi;
}

double ceil(double x)
{
    int xi = (int)x;
    if (x > 0 && x != (double)xi) {
        return xi + 1;
    }
    return xi;
}