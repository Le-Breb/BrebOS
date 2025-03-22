#include "headers.h"

void exec_if(EXEC_PROTOTYPE)
{
    if (ast->type != AST_IF)
        errx(1, "Not an if");

    // Evaluate condition
    exec_list((struct ast*)ast->p1, c);

    // true
    if (c->last_rv == 0)
    {
        exec_switch((struct ast*)ast->p2, c);
        return;
    }

    // false
    if (ast->p3)
    {
        exec_switch((struct ast*)ast->p3, c);
    }
    else
    {
        c->last_rv = 0;
    }
}
