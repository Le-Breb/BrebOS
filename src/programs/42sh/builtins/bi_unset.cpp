#include "headers.h"

static char **get_options(char **args, char *type)
{
    if (*args != NULL && (*args)[0] == '-')
    {
        if ((*args)[1] == 'v' || (*args)[1] == 'f')
        {
            *type = (*args)[1];
        }
        else
        {
            return NULL;
        }
        int i = 1;
        while ((*args)[i] != 0 && (*args)[i] == *type)
        {
            i++;
        }
        if ((*args)[i] == 0)
        {
            return args + 1;
        }
        return NULL;
    }
    return args;
}

int bi_unset(BI_PROTOTYPE)
{
    char type = 'v';
    char **to_unset = get_options(args + 1, &type);
    if (to_unset == NULL)
    { // wrong option
        fprintf(stderr, "wrong option to 'unset'\n");
        return 1;
    }
    char all_set = 1;
    while (*to_unset != NULL)
    {
        if (type == 'v')
        {
            all_set = all_set && list_get_value(c->list_var, *to_unset) != NULL;
            c->list_var = list_remove(c->list_var, *to_unset);
        }
        if (type == 'f')
        {
            all_set =
                all_set && list_func_get_value(c->list_func, *to_unset) != NULL;
            c->list_func = list_func_remove(c->list_func, *to_unset);
        }
        to_unset++;
    }
    return !all_set;
}
