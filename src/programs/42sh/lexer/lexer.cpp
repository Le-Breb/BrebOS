#define _POSIX_C_SOURCE 200809L

#include "headers.h"

struct token lexer_next_token(struct lexer *lexer, size_t *len);

struct lexer *lexer_new(void)
{
    struct lexer *lexer = (struct lexer*)calloc(1, sizeof(struct lexer));
    lexer->input = io_backend_get_line();
    lexer->pos = 0;
    lexer->status = NONE;
    lexer->next = (struct token){ TOKEN_EMPTY, NULL };
    lexer->sur_next = (struct token){ TOKEN_EMPTY, NULL };
    return lexer;
}

void lexer_free(struct lexer *lexer)
{
    if (!lexer)
        return;

    free((void *)lexer->input);
    if (lexer->next.type == TOKEN_WORD || lexer->next.type == TOKEN_IONUMBER)
    {
        free(lexer->next.value);
    }
    if (lexer->sur_next.type == TOKEN_WORD
        || lexer->sur_next.type == TOKEN_IONUMBER)
    {
        free(lexer->sur_next.value);
    }
    free(lexer);
}

struct token lexer_peek(struct lexer *lexer)
{
    struct token tok = { TOKEN_ERROR, 0 };

    if (!lexer)
        return tok;

    if (lexer->next.type != TOKEN_EMPTY)
    {
        tok = lexer->next;
    }
    else
    {
        size_t len;
        tok = lexer_next_token(lexer, &len);
        lexer->pos += len;
        lexer->next = tok;
    }

    return tok;
}

struct token lexer_peek_2(struct lexer *lexer)
{
    struct token tok = { TOKEN_ERROR, 0 };

    if (!lexer)
        return tok;

    if (lexer->sur_next.type != TOKEN_EMPTY)
    {
        tok = lexer->sur_next;
    }
    else
    {
        lexer_peek(lexer);
        size_t len;
        tok = lexer_next_token(lexer, &len);
        lexer->pos += len;
        lexer->sur_next = tok;
    }

    return tok;
}

struct token lexer_pop(struct lexer *lexer)
{
    struct token next_token = { TOKEN_ERROR, 0 };

    if (!lexer)
        return next_token;

    if (lexer->next.type != TOKEN_EMPTY)
    {
        next_token = lexer->next;
        lexer->next = lexer->sur_next;
        lexer->sur_next = (struct token){ TOKEN_EMPTY, NULL };
    }
    else
    {
        size_t len;
        next_token = lexer_next_token(lexer, &len);
        lexer->pos += len;
    }

    return next_token;
}
