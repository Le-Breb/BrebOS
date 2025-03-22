#include "headers.h"

struct ast *parse_element(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;

    // WORD
    struct token t = lexer_peek(lexer);
    if (wordify(&t, WORDIFY_PEEK))
    {
        t = lexer_pop(lexer);
        wordify(&t, WORDIFY_POP);
        struct ast *res = ast_new(AST_STRING);
        res->p1 = t.value;
        return res;
    }
    // redirection
    return parse_redirection(status, lexer);
}
