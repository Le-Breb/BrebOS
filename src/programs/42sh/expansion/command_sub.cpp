#include "expansion/expansion.h"
#include "headers.h"

static void read_until_char(const char **__input, size_t *__size, char c)
{
    const char *start = *__input + 1;
    size_t size = *__size + 1;

    while (*start)
    {
        if (*start == c)
        {
            *__size = size + 1;
            *__input = start + 1;
            return;
        }
        else if (*start == '\\')
        {
            size++;
            start++;
            if (*start == c)
            {
                size++;
                start++;
                continue;
            }
        }
        else if (*start == '\'' || *start == '"' || *start == '`')
        {
            read_until_char(&start, &size, *start);
            continue;
        }
        size++;
        start++;
    }
}

static char sub_substitution(EXPAND_PROTOTYPE)
{
    UNUSED(c);
    UNUSED(final_argsp);
    UNUSED(fargs_ip);
    UNUSED(cur_argp);
    errx(1, "Substitution execution not ported to BrebOS");
    return 0;
#ifdef IMPLEMENTED
    //if (**cur_argp != '(')
    //    return 0;
//
    //char **final_args = *final_argsp;
    //size_t fargs_i = *fargs_ip;
//
    //const char *cur_arg = *cur_argp;
    //size_t len = 0;
//
    //read_until_char(&cur_arg, &len, ')');
    //char *cmd_sub = strndup(*cur_argp + 1, len - 2); // Don't copy '(' and ')'
    //char *cmd = substitute(cmd_sub, c->exec_path);
//
    //append_str(&final_args, fargs_i, cmd);
    //free(cmd_sub);
    //free(cmd);
    //UPDATE_POINTERS(final_args, fargs_i, len);
    //return 1;
#endif
}

static char sub_shell_var(char ***final_argsp, size_t *fargs_ip,
                          char **cur_argp, struct context *c)
{
    char **final_args = *final_argsp;
    size_t fargs_i = *fargs_ip;

    char *__var = strndup(*cur_argp, 1);
    char **expanded_var = expand_var(__var, c);
    size_t dollar_len = strcspn(*cur_argp, " $\"");

    if (!expanded_var)
    {
        free(__var);
        __var = strndup(*cur_argp, dollar_len);
        expanded_var = expand_var(__var, c);
    }

    char **__to_free = expanded_var;

    if (!expanded_var)
    {
        free(__var);
        return 0;
    }

    free(__var);

    while (*expanded_var)
    {
        append_str(&final_args, fargs_i, *expanded_var);
        free(*expanded_var);

        if (!*(++expanded_var))
            break;
        final_args = (char**)realloc(final_args, sizeof(char *) * (++fargs_i + 1));
        final_args[fargs_i] = (char*)calloc(5, 1);
    }

    free(__to_free);
    UPDATE_POINTERS(final_args, fargs_i, dollar_len);
    return 1;
}

// len according to name var regex
static size_t strlen_var(const char *s)
{
    if (!s || !*s || (!isalpha(*s) && *s != '_'))
        return 0;

    size_t len = 1;
    while (s[len] && (isalnum(s[len]) || s[len] == '_'))
        len++;
    return len;
}

static char sub_user_var(char ***final_argsp, size_t *fargs_ip, char **cur_argp,
                         struct context *c)
{
    char **final_args = *final_argsp;
    size_t fargs_i = *fargs_ip;

    size_t dollar_len = strlen_var(*cur_argp);
    char *__var;
    bool brackets = false;
    if (dollar_len == 0)
    {
        dollar_len = strlen_var(*cur_argp + 1);
        __var = strndup(*cur_argp + 1, dollar_len);
        brackets = true;
    }
    else
        __var = strndup(*cur_argp, dollar_len);

    struct list_var *list = c->list_var;

    char *__value = list_get_value(list, __var);
    if (!__value)
    {
        free(__var);
        return 0;
    }

    append_str(&final_args, fargs_i, __value);

    free(__var);
    if (brackets)
        dollar_len += 2;
    UPDATE_POINTERS(final_args, fargs_i, dollar_len);

    return 1;
}

static char sub_env_var(char ***final_argsp, size_t *fargs_ip, char **cur_argp,
                        struct context *c)
{
    char **final_args = *final_argsp;
    size_t fargs_i = *fargs_ip;

    size_t dollar_len = strcspn(*cur_argp, " $\"");
    char *__var = strndup(*cur_argp, dollar_len);

    char **expanded_var = expand_env(__var, c);
    if (!expanded_var)
    {
        *cur_argp += dollar_len;
        free(__var);
        return 0;
    }

    free(__var);

    append_str(&final_args, fargs_i, *expanded_var);

    free(*expanded_var);
    free(expanded_var);

    UPDATE_POINTERS(final_args, fargs_i, dollar_len);
    return 1;
}

void expand_dollar(char ***final_argsp, size_t *fargs_ip, char **cur_argp,
                   struct context *c)
{
    char (*sub[])(EXPAND_PROTOTYPE) = { sub_substitution, sub_shell_var,
                                        sub_user_var, sub_env_var };

    for (size_t i = 0; i < sizeof(sub) / sizeof(*sub); ++i)
        if ((*sub[i])(final_argsp, fargs_ip, cur_argp, c))
            return;
}

#ifdef IMPLEMENTED
static void interprete_backslash(char **cmd_sub, size_t len)
{
    char *cmd = *cmd_sub;
    size_t total_len = 0;
    size_t cur_len = 0;
    while (*cmd)
    {
        cur_len = strcspn(cmd, "\\");
        cmd += cur_len;
        if (*cmd != '\0')
        {
            cur_len++;
            cmd++;
        }
        total_len += cur_len;
        if (*cmd == '$' || *cmd == '`' || *cmd == '\\')
        {
            memmove(cmd - 1, cmd, len - total_len + 1);
        }
    }
}
#endif

void expand_backtick(char ***final_argsp, size_t *fargs_ip, char **cur_argp,
                     struct context *c)
{
    UNUSED(c);
    UNUSED(final_argsp);
    UNUSED(fargs_ip);
    UNUSED(cur_argp);

    errx(EXIT_FAILURE, "Expand backtick not implemented");
#ifdef IMPLEMENTED
    //char **final_args = *final_argsp;
    //size_t fargs_i = *fargs_ip;
//
    //const char *cur_arg = *cur_argp;
    //size_t len = 0;
//
    //read_until_char(&cur_arg, &len, '`');
    //char *cmd_sub = strndup(*cur_argp, len - 1);
    //interprete_backslash(&cmd_sub, len - 1);
    //char *cmd = substitute(cmd_sub, c->exec_path);
//
    //append_str(&final_args, fargs_i, cmd);
    //free(cmd_sub);
    //free(cmd);
    //UPDATE_POINTERS(final_args, fargs_i, len);
#endif
}
