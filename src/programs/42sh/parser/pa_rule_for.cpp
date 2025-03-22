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

char **get_words(enum parser_status *status, struct lexer *lexer)
{
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_IN)
    {
        *status = PARSER_UNEXPECTED_TOKEN;
        return NULL;
    }
    lexer_pop(lexer);
    t = lexer_peek(lexer);
    char **res = NULL;
    size_t i = 0;
    while (wordify(&t, WORDIFY_PEEK))
    {
        t = lexer_pop(lexer);
        wordify(&t, WORDIFY_POP);
        res = (char**)realloc(res, sizeof(char *) * (i + 2));
        res[i++] = t.value;
        res[i] = NULL;
        t = lexer_peek(lexer);
    }
    return res;
}

struct ast *aux_for_do(PARSER_PROTOTYPE, struct ast *res)
{
    // do
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_DO)
        return handle_error_parser(status, res);
    lexer_pop(lexer);

    // command
    res->p1 = parse_compound_list(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
        return handle_error_parser(status, res);

    // done
    t = lexer_peek(lexer);
    if (t.type != TOKEN_DONE)
        return handle_error_parser(status, res);

    if (res->p3 == NULL)
    {
        res->p3 = calloc(2, sizeof(char *));
        ((char **)res->p3)[0] = (char*)malloc(10);
        strcpy(((char **)res->p3)[0], "$@");
    }

    lexer_pop(lexer);
    return res;
}

struct ast *parse_rule_for(PARSER_PROTOTYPE)
{
    // for
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_FOR)
        return handle_error_parser(status, NULL);
    struct ast *res = ast_new(AST_FOR);
    lexer_pop(lexer);

    // word
    t = lexer_peek(lexer);
    if (!wordify(&t, WORDIFY_PEEK))
        return handle_error_parser(status, res);
    t = lexer_pop(lexer);
    wordify(&t, WORDIFY_POP);
    res->p2 = t.value;

    t = lexer_peek(lexer);
    if (t.type == TOKEN_SEMICOLON) // cas 1: for word; do list done
    {
        lexer_pop(lexer);
        skip_newlines(lexer);
        return aux_for_do(status, lexer, res);
    }
    skip_newlines(lexer);
    t = lexer_peek(lexer);
    if (t.type == TOKEN_DO)
    { // cas 2: for word do list done
        return aux_for_do(status, lexer, res);
    }
    res->p3 = get_words(status, lexer);
    t = lexer_peek(lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN
        || (t.type != TOKEN_SEMICOLON && t.type != TOKEN_NEWLINE))
        return handle_error_parser(status, res);
    lexer_pop(lexer); // cas 3/4: for word in [words] (';' / '\n') do list done
    skip_newlines(lexer);
    return aux_for_do(status, lexer, res);
}
