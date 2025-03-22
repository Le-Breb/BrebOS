#include "headers.h"

struct ast *handle_error_parser(enum parser_status *status, struct ast *ast)
{
    *status = PARSER_UNEXPECTED_TOKEN;
    // fprintf(stderr, "Error during parsing\n");
    if (ast)
        ast_free(ast);
    return NULL;
}

struct ast *try_rule(PARSER_PROTOTYPE, struct ast *(*fun)(PARSER_PROTOTYPE))
{
    *status = PARSER_OK;
    struct ast *ast = fun(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
    {
        ast_free(ast);
        *status = PARSER_OK;
        return NULL;
    }
    return ast;
}

char wordify(struct token *t, enum wordify_type type)
{
    static struct token_model arr[256] = {
        { "if", TOKEN_IF },       { "then", TOKEN_THEN },
        { "else", TOKEN_ELSE },   { "elif", TOKEN_ELIF },
        { "fi", TOKEN_FI },       { "do", TOKEN_DO },
        { "done", TOKEN_DONE },   { "case", TOKEN_CASE },
        { "esac", TOKEN_ESAC },   { "while", TOKEN_WHILE },
        { "until", TOKEN_UNTIL }, { "for", TOKEN_FOR },
        { "in", TOKEN_IN },       { "!", TOKEN_NOT },
        { "{", TOKEN_LBRACE },    { "}", TOKEN_RBRACE },
        { "", TOKEN_ERROR }, // needs to stay at the end (see loop below)
    };
    int i = 0;
    while (arr[i].type != TOKEN_ERROR && arr[i].type != t->type)
    {
        i++;
    }
    if (arr[i].type != TOKEN_ERROR)
    {
        t->type = TOKEN_WORD;
        if (type == WORDIFY_POP)
        {
            t->value = (char*)malloc(256);
            strcpy(t->value, arr[i].str);
        }
    }
    return t->type == TOKEN_WORD;
}

struct ast *parse_input(PARSER_PROTOTYPE)
{
    struct token t = lexer_peek(lexer);
    if (t.type == TOKEN_ERROR)
        return handle_error_parser(status, NULL);

    // '\n' and EOF rules
    if (t.type == TOKEN_EOF || t.type == TOKEN_NEWLINE)
    {
        if (t.type == TOKEN_EOF)
        {
            *status = PARSER_EOF;
        }
        lexer_pop(lexer);
        return NULL;
    }

    // list
    struct ast *ast = parse_list(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
    {
        return handle_error_parser(status, ast);
    }
    t = lexer_peek(lexer);
    if (t.type == TOKEN_EOF)
    {
        lexer_pop(lexer);
        *status = PARSER_EOF;
        return ast;
    }
    if (t.type == TOKEN_NEWLINE)
    {
        lexer_pop(lexer);
        return ast;
    }

    return handle_error_parser(status, ast);
}
