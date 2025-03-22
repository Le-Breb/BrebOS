#ifndef PARSER_H
#define PARSER_H

#include "ast/ast.h"
#include "lexer/lexer.h"
#include "lexer/token.h"

enum parser_status
{
    PARSER_OK,
    PARSER_EOF,
    PARSER_UNEXPECTED_TOKEN,
};

enum wordify_type
{
    WORDIFY_PEEK,
    WORDIFY_POP,
};

#define PARSER_PROTOTYPE enum parser_status *status, struct lexer *lexer

struct ast *handle_error_parser(enum parser_status *status, struct ast *ast);
struct ast *try_rule(PARSER_PROTOTYPE, struct ast *(*fun)(PARSER_PROTOTYPE));
char wordify(struct token *t, enum wordify_type type);

struct ast *parse_input(PARSER_PROTOTYPE);
struct ast *parse_list(PARSER_PROTOTYPE);
struct ast *parse_and_or(PARSER_PROTOTYPE);
struct ast *parse_pipeline(PARSER_PROTOTYPE);
struct ast *parse_command(PARSER_PROTOTYPE);

struct ast *parse_simple_command(PARSER_PROTOTYPE);
struct ast *parse_shell_command(PARSER_PROTOTYPE);
struct ast *parse_funcdec(PARSER_PROTOTYPE);
struct ast *parse_redirection(PARSER_PROTOTYPE);
struct ast *parse_prefix(PARSER_PROTOTYPE);

struct ast *parse_element(PARSER_PROTOTYPE);
struct ast *parse_compound_list(PARSER_PROTOTYPE);
struct ast *parse_rule_for(PARSER_PROTOTYPE);
struct ast *parse_rule_while(PARSER_PROTOTYPE);
struct ast *parse_rule_until(PARSER_PROTOTYPE);

struct ast *parse_rule_case(PARSER_PROTOTYPE);
struct ast *parse_rule_if(PARSER_PROTOTYPE);
struct ast *parse_else_clause(PARSER_PROTOTYPE);
struct ast *parse_case_clause(PARSER_PROTOTYPE);
struct ast *parse_case_item(PARSER_PROTOTYPE);

struct ast *parse_subshell(PARSER_PROTOTYPE);

#endif /* !PARSER_H */
