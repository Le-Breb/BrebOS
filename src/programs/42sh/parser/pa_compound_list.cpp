#include "headers.h"

static void skip_newlines(struct lexer *lexer)
{
    struct token t = lexer_peek(lexer);
    while (t.type == TOKEN_NEWLINE)
    {
        lexer_pop(lexer);
        t = lexer_peek(lexer);
    }
}

struct ast *parse_compound_list(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;
    struct ast *res = ast_new(AST_LIST);

    // '\n'
    skip_newlines(lexer);

    // and_or
    res->p1 = parse_and_or(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
    {
        return handle_error_parser(status, res);
    }

    struct ast *cur = res;
    struct token t = lexer_peek(lexer);
    while (t.type == TOKEN_SEMICOLON || t.type == TOKEN_NEWLINE)
    {
        lexer_pop(lexer);
        skip_newlines(lexer);

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

    skip_newlines(lexer);
    return res;
}
