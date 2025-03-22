#include "expansion/expansion.h"
#include "headers.h"

static void add_backslash(char ***final_argsp, size_t *fargs_ip,
                          char **cur_argp, struct context *c)
{
    UNUSED(cur_argp);
    UNUSED(c);
    char **final_args = *final_argsp;
    size_t fargs_i = *fargs_ip;

    append_str(&final_args, fargs_i, "\\");
    UPDATE_POINTERS(final_args, fargs_i, 0);
}

static char normal_backshlash_behavior(char c)
{
    const char *special_char = "$`\"\\\n";
    const char* sp = special_char;
    while (*sp)
    {
        if (*sp++ == c)
            return 1;
    }
    return 0;
}

static size_t get_next_quote_len(char *input)
{
    size_t quote_len = 0;

    while (input[quote_len] && input[quote_len] != '\"')
    {
        quote_len += strcspn(input, "\"");
        if (quote_len == 1 && input[quote_len - 1] == '\\')
            quote_len += 1;
        else if (quote_len > 1 && input[quote_len - 1] == '\\'
                 && input[quote_len - 2] != '\\')
            quote_len += 1;
    }

    return quote_len;
}

void expand_double(char ***final_argsp, size_t *fargs_ip, char **cur_argp,
                   struct context *c)
{
    char **final_args = *final_argsp;
    char *cur_input = *cur_argp;

    size_t fargs_i = *fargs_ip;
    size_t quote_len = get_next_quote_len(cur_input);

    char *__var = strndup(*cur_argp, quote_len);
    char *__free_var = __var;

    while (*__var)
    {
        size_t len = strcspn(__var, "\\$");
        append_nstr(&final_args, fargs_i, __var, &len);
        __var += len;
        switch (*__var)
        {
        case '\\':
            __var++;
            if (!normal_backshlash_behavior(*__var))
                add_backslash(&final_args, &fargs_i, cur_argp, c);
            expand_backslash(&final_args, &fargs_i, &__var, c);
            break;
        case '$':
            __var++;
            expand_dollar(&final_args, &fargs_i, &__var, c);
            break;
        default:
            continue;
        }
    }

    free(__free_var);
    UPDATE_POINTERS(final_args, fargs_i, quote_len + 1);
}
