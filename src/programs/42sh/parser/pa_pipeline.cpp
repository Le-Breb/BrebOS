#include "headers.h"

static struct token skip_newlines(struct lexer *lexer)
{
    struct token t = lexer_peek(lexer);
    while (t.type == TOKEN_NEWLINE)
    {
        lexer_pop(lexer);
        t = lexer_peek(lexer);
    }
    return t;
}

struct ast *parse_pipeline_rec(PARSER_PROTOTYPE)
{
    // '|' {'\n'} check before going in rec

    *status = PARSER_OK;

    struct ast *pipeline = ast_new(AST_PIPE);

    // command
    pipeline->p1 = parse_command(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
        return handle_error_parser(status, pipeline);

    struct token t = lexer_peek(lexer);
    // |
    if (t.type == TOKEN_PIPE)
    {
        lexer_pop(lexer);
        // \n
        skip_newlines(lexer);
        // 1 command or more
        pipeline->p2 = parse_pipeline_rec(status, lexer);
        if (*status == PARSER_UNEXPECTED_TOKEN)
            return handle_error_parser(status, pipeline);
    }
    return pipeline;
}

// structure:
//          neg             pipeline
//          p1                p1  p2
// pipeline or command      command  pipeline
// return neg or pipeline or command node
struct ast *parse_pipeline(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;

    struct ast *neg = NULL;
    struct token t = lexer_peek(lexer);
    // !
    if (t.type == TOKEN_NOT)
    {
        lexer_pop(lexer);
        neg = ast_new(AST_NOT);
    }

    // command
    struct ast *command = parse_command(status, lexer);
    if (*status == PARSER_UNEXPECTED_TOKEN)
    {
        ast_free(neg);
        return handle_error_parser(status, command);
    }

    t = lexer_peek(lexer);
    // |
    if (t.type == TOKEN_PIPE)
    {
        struct ast *pipeline = ast_new(AST_PIPE);
        pipeline->p1 = command;
        if (neg)
            neg->p1 = pipeline;

        lexer_pop(lexer);
        // \n
        skip_newlines(lexer);
        // 1 command or more
        pipeline->p2 = parse_pipeline_rec(status, lexer);
        if (*status == PARSER_UNEXPECTED_TOKEN)
        {
            ast_free(neg);
            return handle_error_parser(status, pipeline);
        }
        return neg ? neg : pipeline;
    }
    if (neg)
        neg->p1 = command;
    return neg ? neg : command;
}
