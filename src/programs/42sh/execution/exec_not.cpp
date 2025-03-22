#include "headers.h"

void exec_not(EXEC_PROTOTYPE)
{
    exec_switch((struct ast*)ast->p1, c);
    c->last_rv = c->last_rv == 1 ? 0 : 1;
}
