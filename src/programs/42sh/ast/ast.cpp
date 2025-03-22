#include "headers.h"

struct ast *ast_new(enum ast_type type)
{
    struct ast *new_ast = (struct ast*)calloc(1, sizeof(struct ast));
    new_ast->type = type;
    new_ast->p1 = NULL;
    new_ast->p2 = NULL;
    new_ast->p3 = NULL;
    return new_ast;
}

void free_argv(char **argv)
{
    if (argv == NULL)
    {
        return;
    }
    int i = 0;
    while (argv[i] != NULL)
    {
        free(argv[i++]);
    }
    free(argv);
}

static void ast_free_2(struct ast *ast)
{
    switch (ast->type)
    {
    case AST_ASSIGN:
        free(ast->p1); // char* from token
        ast_free((struct ast*)ast->p2);
        break;
    case AST_FOR:
        ast_free((struct ast*)ast->p1);
        free(ast->p2);
        free_argv((char**)ast->p3);
        break;
    case AST_FUNCDEF:
        free(ast->p1);
        ast_free((struct ast*)ast->p2);
        break;
    default:
        errx(7, "unknown ast type in ast_free\n");
    }
    free(ast);
}

void ast_free(struct ast *ast)
{
    if (ast == NULL)
        return;

    switch (ast->type)
    {
    case AST_ARGV:
        free_argv((char**)ast->p1); // char**
        break;
    case AST_PIPE:
    case AST_LIST:
    case AST_IF:
    case AST_AND:
    case AST_OR:
    case AST_WHILE:
    case AST_UNTIL:
    case AST_SUBSHELL:
    case AST_NOT:
        ast_free((struct ast*)ast->p1);
        ast_free((struct ast*)ast->p2);
        ast_free((struct ast*)ast->p3);
        break;
    case AST_REDIR:
        ast_free_redir_info((struct redir_info*)ast->p1); // struct redir_info* info
        ast_free((struct ast*)ast->p2); // ast* other redirection
        ast_free((struct ast*)ast->p3); // ast* command
        break;
    default:
        ast_free_2(ast);
        return;
    }
    free(ast);
}

void ast_free_redir_info(struct redir_info *redir_info)
{
    if (redir_info == NULL)
        return;

    free(redir_info->word);
    free(redir_info);
}
