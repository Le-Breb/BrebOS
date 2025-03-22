#include "headers.h"

struct ast *parse_rule_until(PARSER_PROTOTYPE)
{
    // until
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_UNTIL)
        return handle_error_parser(status, NULL);
    struct ast *res = ast_new(AST_UNTIL);
    lexer_pop(lexer);

    // condition
    res->p1 = parse_compound_list(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
        return handle_error_parser(status, res);

    // do
    t = lexer_peek(lexer);
    if (t.type != TOKEN_DO)
        return handle_error_parser(status, res);
    lexer_pop(lexer);

    // true
    res->p2 = parse_compound_list(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
        return handle_error_parser(status, res);

    // done
    t = lexer_peek(lexer);
    if (t.type != TOKEN_DONE)
        return handle_error_parser(status, res);
    lexer_pop(lexer);

    return res;
}
