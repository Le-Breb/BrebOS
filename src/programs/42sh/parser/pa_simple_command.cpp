#include "headers.h"

// return a head of ast of prefix
static struct ast *get_prefix(PARSER_PROTOTYPE)
{
    // prefix
    struct ast *prefix = try_rule(status, lexer, parse_prefix);
    if (!prefix)
        return NULL;

    prefix->p2 = get_prefix(status, lexer);
    return prefix;
}

// return a head of ast of redirections then argv
static struct ast *get_args(enum parser_status *status, struct lexer *lexer)
{
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_WORD)
        return NULL;

    t = lexer_pop(lexer);

    int argc = 1;
    char **argv = (char**)malloc(sizeof(char *) * 2);
    argv[0] = t.value;
    struct ast *head = NULL;
    struct ast *cur = NULL;

    // elements
    struct ast *elt = try_rule(status, lexer, parse_element);
    while (elt)
    {
        if (elt->type == AST_STRING)
        {
            argv = (char**)realloc(argv, sizeof(char *) * (argc + 2));
            argv[argc++] = (char*)elt->p1;
            free(elt); // does not free the string
        }
        else if (elt->type == AST_REDIR)
        {
            if (!head)
            {
                head = elt;
                cur = head;
            }
            else
            {
                cur->p2 = elt;
                cur = (struct ast*)cur->p2;
            }
        }
        else
        {
            free(argv);
            return handle_error_parser(status, head);
        }

        elt = try_rule(status, lexer, parse_element);
    }
    argv[argc] = NULL;
    struct ast *ast_argv = ast_new(AST_ARGV);
    ast_argv->p1 = argv;
    if (!cur)
        return ast_argv;
    cur->p2 = ast_argv;
    return head;
}

struct ast *parse_simple_command(PARSER_PROTOTYPE)
{
    // list of prefixs
    struct ast *head = get_prefix(status, lexer);

    // WORD
    struct token t = lexer_peek(lexer);
    if (t.type != TOKEN_WORD)
        return head ? head : handle_error_parser(status, NULL);
    // get argv and list of redirections
    struct ast *next = get_args(status, lexer);
    if (!next)
        return handle_error_parser(status, head);
    if (head)
    {
        struct ast *cur = head;
        while (cur->p2)
            cur = (struct ast*)cur->p2;

        cur->p2 = next;
    }
    else
        head = next;
    return head;
}
