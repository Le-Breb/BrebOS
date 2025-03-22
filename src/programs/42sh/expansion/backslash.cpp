#include "headers.h"

void expand_backslash(char ***final_argsp, size_t *fargs_ip, char **cur_argp,
                      struct context *c)
{
    UNUSED(c);
    char **final_args = *final_argsp;
    size_t fargs_i = *fargs_ip;
    final_args[fargs_i] =
        (char*)realloc(final_args[fargs_i], strlen(final_args[fargs_i]) + 2 + 5);
    if (**cur_argp == '\0')
    {
        strcat(final_args[fargs_i], "\\");
    }
    else if (**cur_argp == '\n')
    {
        *cur_argp += 1;
    }
    else
    {
        strncat(final_args[fargs_i], *cur_argp, 1);
        *cur_argp += 1;
    }
    *final_argsp = final_args;
}
