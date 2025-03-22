#include "headers.h"

struct ast *parse_shell_command(PARSER_PROTOTYPE)
{
    *status = PARSER_OK;
    struct ast *res = try_rule(status, lexer, parse_rule_if);
    if (res != NULL)
    {
        return res;
    }
    res = try_rule(status, lexer, parse_rule_while);
    if (res != NULL)
    {
        return res;
    }
    res = try_rule(status, lexer, parse_rule_until);
    if (res != NULL)
    {
        return res;
    }
    res = try_rule(status, lexer, parse_rule_for);
    if (res != NULL)
    {
        return res;
    }
    res = try_rule(status, lexer, parse_subshell);
    if (res != NULL)
        return res;
    struct token t = lexer_peek(lexer);
    if (t.type == TOKEN_LBRACE)
    {
        lexer_pop(lexer);
        res = parse_compound_list(status, lexer);
        t = lexer_pop(lexer);
        if (t.type != TOKEN_RBRACE)
        {
            return handle_error_parser(status, res);
        }
        return res;
    }

    return NULL;
}
