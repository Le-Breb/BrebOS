#include "headers.h"

void exec_while(EXEC_PROTOTYPE)
{
    // p1: condition
    // p2: true
    int rv = 0;
    exec_switch((struct ast*)ast->p1, c);
    while (c->last_rv == 0)
    {
        c->loop_level++;
        exec_switch((struct ast*)ast->p2, c);
        rv = c->last_rv;
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
        exec_switch((struct ast*)ast->p1, c);
    }
    c->last_rv = rv;
}
