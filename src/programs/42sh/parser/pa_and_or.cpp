#include "headers.h"

struct ast *get_next(PARSER_PROTOTYPE)
{
    struct token t = lexer_peek(lexer);
    if (t.type == TOKEN_AND || t.type == TOKEN_OR)
    {
        enum token_type aotype = t.type;
        lexer_pop(lexer);
        t = lexer_peek(lexer);
        while (t.type == TOKEN_NEWLINE)
        {
            lexer_pop(lexer);
            t = lexer_peek(lexer);
        }
        struct ast *command = parse_pipeline(status, lexer);
        if (*status == PARSER_UNEXPECTED_TOKEN)
        {
            return handle_error_parser(status, command);
        }
        else
        {
            struct ast *res = ast_new(aotype == TOKEN_OR ? AST_OR : AST_AND);
            res->p1 = NULL;
            res->p2 = command;
            return res;
        }
    }
    return NULL;
}

struct ast *parse_and_or(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;

    // pipeline
    struct ast *res = parse_pipeline(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
        return handle_error_parser(status, res);

    // others
    struct ast *parent = get_next(status, lexer);
    while (parent != NULL)
    {
        parent->p1 = res;
        res = parent;
        parent = get_next(status, lexer);
    }
    return res;
}
