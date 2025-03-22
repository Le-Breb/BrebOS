#include "headers.h"

struct ast *parse_else_clause(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;
    struct token t = lexer_peek(lexer);

    // else
    if (t.type == TOKEN_ELSE)
    {
        lexer_pop(lexer);
        return parse_compound_list(status, lexer);
    }

    // elif
    else if (t.type == TOKEN_ELIF)
    {
        lexer_pop(lexer);
        struct ast *res = ast_new(AST_IF);

        // condition
        res->p1 = parse_compound_list(status, lexer);
        if (*status == PARSER_UNEXPECTED_TOKEN)
            return handle_error_parser(status, res);

        // then
        t = lexer_pop(lexer);
        if (t.type != TOKEN_THEN)
            return handle_error_parser(status, res);

        // true
        res->p2 = parse_compound_list(status, lexer);
        if (*status == PARSER_UNEXPECTED_TOKEN)
            return handle_error_parser(status, res);

        // else_clause
        res->p3 = parse_else_clause(status, lexer);
        if (*status == PARSER_UNEXPECTED_TOKEN)
        {
            *status = PARSER_OK;
        }
        return res;
    }

    return handle_error_parser(status, NULL);
}
