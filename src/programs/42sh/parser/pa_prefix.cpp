#include "headers.h"

static int equal_in(char *s)
{
    while (*s)
    {
        if (*s == '=')
            return 1;
        s++;
    }
    return 0;
}

struct ast *parse_prefix(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;

    // ASSIGNMENT_WORD
    struct token t = lexer_peek(lexer);
    if (t.type == TOKEN_WORD && equal_in(t.value))
    {
        t = lexer_pop(lexer);
        struct ast *res = ast_new(AST_ASSIGN);
        res->p1 = t.value;
        return res;
    }
    // redirection
    return parse_redirection(status, lexer);
}
