#include "headers.h"

void exec_list(EXEC_PROTOTYPE)
{
    if (!ast)
        return;
    if (ast->type != AST_LIST)
        errx(1, "Not a command list");

    exec_switch((struct ast*)ast->p1, c);
    exec_list((struct ast*)ast->p2, c);
}
