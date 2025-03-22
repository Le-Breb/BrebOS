#include "headers.h"

struct ast *parse_rule_case(PARSER_PROTOTYPE)
{
    UNUSED(status);
    UNUSED(lexer);
    return ast_new(AST_STRING);
}
