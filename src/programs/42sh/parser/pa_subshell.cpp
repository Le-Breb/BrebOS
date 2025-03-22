#include "headers.h"

struct ast *parse_subshell(PARSER_PROTOTYPE)
{
    // (
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_LPAREN)
        return handle_error_parser(status, NULL);
    lexer_pop(lexer);

    // compound_list
    struct ast *ast = parse_compound_list(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
        return handle_error_parser(status, ast);

    // )
    t = lexer_peek(lexer);
    if (t.type != TOKEN_RPAREN)
        return handle_error_parser(status, ast);
    lexer_pop(lexer);

    struct ast *subshell = ast_new(AST_SUBSHELL);
    subshell->p1 = ast;

    return subshell;
}
