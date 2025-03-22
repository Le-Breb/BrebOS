#include "headers.h"

struct ast *parse_list(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;
    struct ast *res = ast_new(AST_LIST);

    // and_or
    res->p1 = parse_and_or(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
    {
        return handle_error_parser(status, res);
    }

    struct ast *cur = res;
    struct token t = lexer_peek(lexer);
    while (t.type == TOKEN_SEMICOLON)
    {
        lexer_pop(lexer);

        struct ast *command = parse_and_or(status, lexer);
        if (*status == PARSER_UNEXPECTED_TOKEN)
        {
            *status = PARSER_OK;
            break; // we were in the [ ';' ] clause
        }
        else
        {
            cur->p2 = ast_new(AST_LIST);
            cur = (struct ast*)cur->p2;
            cur->p1 = command; // other and_or
        }

        t = lexer_peek(lexer);
    }
    cur->p2 = NULL;
    return res;
}
