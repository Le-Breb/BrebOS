#include "headers.h"

struct ast *parse_rule_if(PARSER_PROTOTYPE)
{
    // if
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_IF)
        return handle_error_parser(status, NULL);
    struct ast *res = ast_new(AST_IF);
    lexer_pop(lexer);

    // condition
    res->p1 = parse_compound_list(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
        return handle_error_parser(status, res);

    // then
    t = lexer_peek(lexer);
    if (t.type != TOKEN_THEN)
        return handle_error_parser(status, res);
    lexer_pop(lexer);

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

    // fi
    t = lexer_peek(lexer);
    if (t.type != TOKEN_FI)
        return handle_error_parser(status, res);
    lexer_pop(lexer);

    return res;
}
