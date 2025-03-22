#include "shell_vars.h"

static char **get_args(struct context *c)
{
    int argc = c->argc;
    char **args = (char**)calloc(argc + 2, sizeof(char *));
    char **__args = args;
    char **argv = c->argv;

    for (int i = 1; i <= argc; i++)
    {
        if (!argv[i])
            break;
        *args++ = strdup(argv[i]);
    }

    return __args;
}

static char **get_args_gathered(struct context *c)
{
    char **args = get_args(c);

    char **arg = args;
    size_t tot_len = 0;
    int argc = 0;
    while (*arg)
    {
        tot_len += strlen(*arg++);
        argc++;
    }
    arg = args;
    char *args_gathered = (char*)calloc(tot_len + argc + 5, sizeof(char));
    if (*arg)
    {
        strcat(args_gathered, *arg);
        free(*arg++);
    }
    while (*arg)
    {
        strcat(args_gathered, " ");
        strcat(args_gathered, *arg);
        free(*arg++);
    }
    char **args_list = (char**)calloc(2, sizeof(char *));
    args_list[0] = args_gathered;
    free(args);
    return args_list;
}

static char **num_to_args(int n)
{
    char b[1000];

    sprintf(b, "%d", n);
    char **arr = (char**)malloc(sizeof(char *) * 2);
    arr[0] = strdup(b);
    arr[1] = NULL;
    return arr;
}

static char **get_last_rv(struct context *c)
{
    return num_to_args(c->last_rv);
}

static char **get_pid(void)
{
    return num_to_args(getpid());
}

static char **get_argc(struct context *c)
{
    return num_to_args(c->argc - 1);
}

static char **get_nth_arg(int n, struct context *c)
{
    int argc = c->argc;
    char **args = (char**)calloc(2, sizeof(char *));
    *args = strdup(n >= argc ? "" : c->argv[n]);

    return args;
}

static char **get_rand(void)
{
    return num_to_args(rand());
}

#ifdef IMPLEMENTED
static char **get_uid(void)
{
    return num_to_args(getuid());
}
#endif

char **expand_var(const char *str, struct context *c)
{
    if (!strcmp("@", str))
        return get_args(c);
    if (!strcmp("*", str))
        return get_args_gathered(c);
    if (!strcmp("?", str))
        return get_last_rv(c);
    if (!strcmp("$", str))
        return get_pid();
#ifdef IMPLEMENTED
    if (!strcmp("UID", str))
        return get_uid();
#endif
    if (!strcmp("#", str))
        return get_argc(c);
    if (!strcmp("RANDOM", str))
        return get_rand();
    if (*str && *str >= '0' && *str <= '9')
    {
        int val = 0;
        while (*str && *str >= '0' && *str <= '9')
        {
            val *= 10;
            val += *str - '0';
            str++;
        }

        return !*str ? get_nth_arg(val, c) : NULL;
    }

    return NULL;
}
