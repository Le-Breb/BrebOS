#include "headers.h"

void exec_or(EXEC_PROTOTYPE)
{
    if (ast->type != AST_OR)
        errx(EXIT_FAILURE, "Not an and");
    exec_switch((struct ast*)ast->p1, c);
    if (c->last_rv != 0)
    {
        exec_switch((struct ast*)ast->p2, c);
    }
}
