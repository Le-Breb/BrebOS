#ifndef EXECUTION_H
#define EXECUTION_H

#include "ast/ast.h"
#include "list_var/list_var.h"

enum break_type
{
    NO_BREAK,
    BREAK,
    CONTINUE
};

struct context
{
    const char *exec_path;
    int last_rv;
    int argc;
    char **argv;
    struct list_var *list_var;
    int pipes[2][2];
    int input_pipe_set, pipe_index, number_of_pipes, stdout_save, stdin_save;
    int shall_exit;
    enum break_type break_type;
    int loop_level, break_count;
    struct list_func *list_func;
};

struct context *context_new(const char *argv0);
void context_free(struct context *ctx);

#define EXEC_PROTOTYPE struct ast *ast, struct context *c

void exec_switch(EXEC_PROTOTYPE); // to call the right command based on type

void exec_command(EXEC_PROTOTYPE);
void exec_if(EXEC_PROTOTYPE);
void exec_list(EXEC_PROTOTYPE);
void exec_redir(EXEC_PROTOTYPE);
void exec_pipe(EXEC_PROTOTYPE);
void exec_not(EXEC_PROTOTYPE);
void exec_while(EXEC_PROTOTYPE);
void exec_until(EXEC_PROTOTYPE);
void exec_and(EXEC_PROTOTYPE);
void exec_or(EXEC_PROTOTYPE);
void exec_assign(EXEC_PROTOTYPE);
void exec_for(EXEC_PROTOTYPE);
void exec_funcdef(EXEC_PROTOTYPE);
void exec_case(EXEC_PROTOTYPE);
void exec_alias(EXEC_PROTOTYPE);
void exec_simple_command(EXEC_PROTOTYPE);
void exec_argv(EXEC_PROTOTYPE);
void exec_subshell(EXEC_PROTOTYPE);

#endif /* ! EXECUTION_H */
