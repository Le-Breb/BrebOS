#include "headers.h"

void expand_single(char ***final_argsp, size_t *fargs_ip, char **cur_argp,
                   struct context *c)
{
    UNUSED(c);
    char **final_args = *final_argsp;
    size_t fargs_i = *fargs_ip;
    size_t quote_len = strcspn(*cur_argp, "'");
    final_args[fargs_i] = (char*)realloc(final_args[fargs_i],
                                  strlen(final_args[fargs_i]) + quote_len + 5);
    strncat(final_args[fargs_i], *cur_argp, quote_len);
    *cur_argp = *cur_argp + quote_len + 1;
}
