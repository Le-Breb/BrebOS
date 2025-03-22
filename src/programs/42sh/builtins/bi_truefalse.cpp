#include "headers.h"

int bi_false(BI_PROTOTYPE)
{
    UNUSED(args);
    UNUSED(c);
    return 1;
}

int bi_true(BI_PROTOTYPE)
{
    UNUSED(c);
    UNUSED(args);
    return 0;
}
