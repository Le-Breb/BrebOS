#include "headers.h"

static void (*funs[256])(EXEC_PROTOTYPE);

__attribute__((constructor)) void init()
{
    funs[AST_LIST] = exec_list;
    funs[AST_IF] = exec_if;
#ifdef IMPLEMENTED
    funs[AST_REDIR] = exec_redir;
    funs[AST_PIPE] = exec_pipe;
#endif
    funs[AST_NOT] = exec_not;
    funs[AST_WHILE] = exec_while;
    funs[AST_UNTIL] = exec_until;
    funs[AST_AND] = exec_and;
    funs[AST_OR] = exec_or;
    funs[AST_ASSIGN] = exec_assign;
    funs[AST_FOR] = exec_for;
    funs[AST_FUNCDEF] = exec_funcdef;
    funs[AST_CASE] = exec_case;
    funs[AST_ALIAS] = exec_alias;
    funs[AST_ARGV] = exec_argv;
#ifdef IMPLEMENTED
    funs[AST_SUBSHELL] = exec_subshell;
#endif
}

void exec_switch(EXEC_PROTOTYPE)
{
    if (!ast || c->shall_exit || c->break_count)
        return;

#pragma region k_adapted
    if (ast->type == AST_SUBSHELL || ast->type == AST_REDIR || ast->type == AST_PIPE)
    {
        errx(1, "You just used a 42sh fonctionnality that is not yet ported to BrebOS, sorry");
        return;
    }
#pragma endregion
    funs[ast->type](ast, c);
}
