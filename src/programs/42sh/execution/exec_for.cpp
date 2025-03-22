#include "headers.h"

static void free_args(char **args)
{
    if (args == NULL)
    {
        return;
    }
    size_t i = 0;
    while (args[i] != NULL)
    {
        free(args[i++]);
    }
    free(args);
}

void exec_for(EXEC_PROTOTYPE)
{
    char **values = expand((char ***)(&(ast->p3)), c);
    size_t i = 0;
    char *namecopy = NULL;
    char *valcopy = NULL;
    while (values[i] != NULL)
    {
        c->loop_level++;
        namecopy = (char*)malloc(strlen((char*)ast->p2) + 5);
        namecopy = strcpy(namecopy, (char*)ast->p2);
        valcopy = (char*)malloc(strlen(values[i]) + 5);
        valcopy = strcpy(valcopy, values[i++]);
        c->list_var = list_add(c->list_var, namecopy, valcopy);
        exec_switch((struct ast*)ast->p1, c);
        c->loop_level--;
        if (c->break_count)
        {
            c->break_count--;
            // Outermost loop, reset break counter
            if (!c->loop_level && c->break_count)
                c->break_count = 0;
            // Multi level breaking, break current inner loop
            if (c->break_count)
                break;
            // Reached break/continue target loop
            if (!c->break_count)
            {
                enum break_type break_type = c->break_type;
                c->break_type = NO_BREAK;
                if (break_type == BREAK)
                    break;
            }
        }
    }
    if (i == 0)
    {
        c->last_rv = 0;
    }
    free_args(values);
}
