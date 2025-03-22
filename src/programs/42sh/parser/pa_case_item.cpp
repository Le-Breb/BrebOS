#include "headers.h"

struct ast *parse_case_item(PARSER_PROTOTYPE)
{
    UNUSED(status);
    UNUSED(lexer);
    return ast_new(AST_STRING);
}
