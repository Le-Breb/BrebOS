#include "headers.h"

void exec_and(EXEC_PROTOTYPE)
{
    if (ast->type != AST_AND)
        errx(EXIT_FAILURE, "Not an and");
    exec_switch((struct ast*)ast->p1, c);
    if (c->last_rv == 0)
    {
        exec_switch((struct ast*)ast->p2, c);
    }
}
