#ifndef AST_H
#define AST_H

#include <lexer/token.h>

enum ast_type
{
    AST_STRING, // not an actual type, used to process command arguments
                // p1: char* string

    AST_ARGV, // char** argv

    AST_LIST, // p1: ast* to_exec
              // p2: ast* next

    AST_IF, // p1: ast* condition,
            // p2: ast* true,
            // p3: ast* false

    AST_REDIR,
    AST_PIPE, // p1: ast* command
              // p2: ast* pipe with

    AST_NOT, // p1: ast* ast,

    AST_SUBSHELL, // p1: compound_list

    AST_WHILE,
    AST_UNTIL,
    AST_AND,
    AST_OR,
    AST_ASSIGN,
    AST_FOR,
    AST_FUNCDEF,
    AST_CASE,
    AST_ALIAS,
};

struct redir_info
{
    enum token_type type; // redirection type
    int fd;
    char *word;
};

struct ast
{
    enum ast_type type; // The kind of node we're dealing with
    void *p1;
    void *p2;
    void *p3;
};

struct ast *ast_new(enum ast_type type);

void ast_free(struct ast *ast);

void ast_free_redir_info(struct redir_info *redir_info);

#endif /* !AST_H */
