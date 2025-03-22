#include "headers.h"

static int exit_itoa(const char *s)
{
    int res = 0;
    while (*s && *s >= '0' && *s <= '9')
        res = res * 10 + *s++ - '0';

    return res; // if error return value is undefined so we don't care,
    // wee return whatever there is in res
}

int bi_exit(BI_PROTOTYPE)
{
    c->shall_exit = 1;
    return args[1] ? exit_itoa(args[1]) : c->last_rv;
}
