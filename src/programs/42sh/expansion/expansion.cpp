#include "headers.h"

void append_nstr(char ***final_argsp, size_t index, const char *str, size_t *len)
{
    char **final_args = *final_argsp;
    size_t str_len = len ? *len : strlen(str);

    final_args[index] =
        (char*)realloc(final_args[index], strlen(final_args[index]) + str_len + 5);
    strncat(final_args[index], str, str_len);

    *final_argsp = final_args;
}

void (*funs[256])(EXPAND_PROTOTYPE) = {};
__attribute__((constructor)) void init0()
{
    funs['\\'] = expand_backslash;
    funs['\''] = expand_single;
    funs['\"'] = expand_double;
    funs['`'] = expand_backtick;
    funs['$'] = expand_dollar;
}

static void expand_loop(char ***final_argsp, size_t *fargs_ip, char **cur_argp,
                        struct context *c)
{
    // The final_args[fargs_i] string should be null-terminated after any
    // operation (to use strcat, strlen...) even if it is not fully processed
    // 'ec'ho should be [e,c,\0] after removing the quotes, then [e,c,h,o,\0]
    size_t len;
    char curchar = (*cur_argp)[0];
    switch (curchar)
    {
    case '\'':
    case '\"':
    case '`':
    case '$':
    case '\\':
        *cur_argp = *cur_argp + 1;
        funs[(int)curchar](final_argsp, fargs_ip, cur_argp, c);
        break;
    default:
        len = strcspn(*cur_argp, "`'\\\"$");
        (*final_argsp)[*fargs_ip] =
            (char*)realloc((*final_argsp)[*fargs_ip],
                    strlen((*final_argsp)[*fargs_ip]) + len + 5);
        strncat((*final_argsp)[*fargs_ip], *cur_argp, len);
        *cur_argp = *cur_argp + len;
        break;
    }
}

char **expand(char ***argsp, struct context *c)
{
    char **args = *argsp;
    char **final_args = (char**)malloc(sizeof(char *));
    size_t fargs_i = 0;
    size_t i = 0;
    while (args[i] != NULL)
    {
        final_args[fargs_i] = (char*)calloc(5, 1);
        char *cur_arg = args[i];
        while (*cur_arg != 0)
        {
            expand_loop(&final_args, &fargs_i, &cur_arg, c);
        }
        if (final_args[fargs_i][0] == 0 // for the $* and $@ variables, we can
            && strcspn(args[i], "@*") != strlen(args[i])) // discard empty args
        {
            free(final_args[fargs_i]);
        }
        else
        {
            final_args = (char**)realloc(final_args, sizeof(char *) * (++fargs_i + 1));
        }
        i++;
    }
    final_args[fargs_i] = NULL;
    return final_args;
}
