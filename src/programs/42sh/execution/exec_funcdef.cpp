#include "headers.h"

static char is_name_char(char c, int j)
{
    if ((c <= 'z' && c >= 'a') || (c <= 'Z' && c >= 'A') || c == '_')
    {
        return 1;
    }
    if (j != 0 && (c <= '9' && c >= '0'))
    {
        return 1;
    }
    return 0;
}

char *check_name(char **expanded)
{
    if (expanded[0] == NULL || expanded[1] != NULL)
    {
        int i = 0;
        while (expanded[i] != NULL)
        {
            free(expanded[i++]);
        }
        free(expanded);
        return NULL;
    }
    char *res = expanded[0];
    free(expanded);
    int j = 0;
    while (is_name_char(res[j], j))
    {
        j++;
    }
    if (res[j] == 0)
    {
        return res;
    }
    free(res);
    return NULL;
}

char **expand_one_s(char *original, struct context *c)
{
    char *name = (char*)malloc(strlen(original) + 5);
    strcpy(name, original);
    char **pp = (char**)calloc(2, sizeof(char *));
    pp[0] = name;
    char **expanded = expand(&pp, c);
    free(name);
    free(pp);
    return expanded;
}

void exec_funcdef(EXEC_PROTOTYPE)
{
    char *name = check_name(expand_one_s((char*)ast->p1, c));
    if (name == NULL)
    {
        c->last_rv = 1;
    }
    else
    {
        c->list_func = list_func_add(c->list_func, name, (struct ast*)ast->p2);
        ast->p2 = NULL;
        c->last_rv = 0;
    }
}
