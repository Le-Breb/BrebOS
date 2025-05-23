#include "headers.h"

static int is_digit(const char *s)
{
    while (*s && isdigit(*s))
        s++;
    return !*s;
}

int bi_continue(BI_PROTOTYPE)
{
    // Get number of breaks to perform
    int n = 1;
    if (args[1])
    {
        if (!is_digit(args[1]))
            errx(1, "bi_break: bad argument");
        n = atoi(args[1]);
    }

    c->break_count = n;
    c->break_type = CONTINUE;

    return 0;
}
