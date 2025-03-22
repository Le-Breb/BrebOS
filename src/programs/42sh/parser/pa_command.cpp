#include "headers.h"

static struct ast *get_redirections(PARSER_PROTOTYPE)
{
    struct ast *cur = try_rule(status, lexer, parse_redirection);
    if (!cur)
        return NULL;

    cur->p2 = get_redirections(status, lexer);
    return cur;
}

struct ast *parse_command(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;

    // funcdec
    struct ast *ast = try_rule(status, lexer, parse_funcdec);
    if (ast != NULL)
    {
        return ast;
    }

    // simple_command
    ast = try_rule(status, lexer, parse_simple_command);
    if (ast)
        return ast;
    // shell command
    ast = try_rule(status, lexer, parse_shell_command);
    if (ast)
    {
        struct ast *head = get_redirections(status, lexer);
        if (!head)
            return ast;
        struct ast *cur = head;
        while (cur->p2 != NULL)
            cur = (struct ast*)cur->p2;
        cur->p2 = ast;
        return head;
    }
    return handle_error_parser(status, NULL);
}
