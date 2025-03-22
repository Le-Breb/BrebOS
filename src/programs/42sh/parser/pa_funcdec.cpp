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

struct ast *parse_funcdec(PARSER_PROTOTYPE)
{
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_WORD)
    {
        return handle_error_parser(status, NULL);
    }
    t = lexer_peek_2(lexer);
    if (t.type != TOKEN_LPAREN)
    {
        return handle_error_parser(status, NULL);
    }
    t = lexer_pop(lexer);
    struct ast *res = ast_new(AST_FUNCDEF);
    res->p1 = t.value;
    lexer_pop(lexer);
    t = lexer_peek(lexer);
    if (t.type != TOKEN_RPAREN)
    {
        return handle_error_parser(status, res);
    }
    lexer_pop(lexer);
    skip_newlines(lexer);
    res->p2 = parse_shell_command(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
        return handle_error_parser(status, res);
    return res;
}
