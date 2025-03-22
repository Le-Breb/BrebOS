#include "headers.h"

struct ast *parse_redirection(PARSER_PROTOTYPE)
{
    struct ast *ast = ast_new(AST_REDIR);
    ast->p1 = calloc(1, sizeof(struct redir_info));
    struct redir_info *r = (struct redir_info*)ast->p1;

    // IO number
    struct token tok = lexer_peek(lexer);
    r->fd = -1;
    if (tok.type == TOKEN_IONUMBER)
    {
        tok = lexer_pop(lexer);
        r->fd = atoi(tok.value);
        free(tok.value);
    }

    // Redir operator
    tok = lexer_peek(lexer);
    if (!(tok.type == TOKEN_REDIR_LG || tok.type == TOKEN_REDIR_GO
          || tok.type == TOKEN_REDIR_LA || tok.type == TOKEN_REDIR_GA
          || tok.type == TOKEN_REDIR_GG || tok.type == TOKEN_REDIR_G
          || tok.type == TOKEN_REDIR_L))
    {
        free(ast->p1);
        ast->p1 = NULL;
        return handle_error_parser(status, ast);
    }
    tok = lexer_pop(lexer);
    r->type = tok.type;

    // word
    tok = lexer_peek(lexer);
    if (!wordify(&tok, WORDIFY_PEEK))
        return handle_error_parser(status, ast);
    tok = lexer_pop(lexer);
    wordify(&tok, WORDIFY_POP);
    r->word = tok.value;

    return ast;
}
