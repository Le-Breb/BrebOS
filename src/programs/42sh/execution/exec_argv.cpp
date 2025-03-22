#include "headers.h"

static int run_execvp(char **args)
{
#pragma region k_adapted

    //pid_t pid = fork();
    //if (pid < 0) // flop
    //{
    //    errx(1, "fork failed");
    //}
    //if (pid == 0) // child
    //{
    //    if (execvp(args[0], args) == -1)
    //    {
    //        exit(127);
    //    }
    //}
    //// parent
    //int status;
    //waitpid(pid, &status, 0);
    //int exit_status = WEXITSTATUS(status);
    //if (exit_status == 127)
    //{
    //    fprintf(stderr, "42sh: exec error\n");
    //}
    //return exit_status;
    int argc = 0;
    while (args[argc] != NULL)
        argc++;
    exec(args[0], --argc, (const char**)(args + 1));
    return 0;
#pragma endregion
}

static int (*get_bi_function(int id))(BI_PROTOTYPE)
{
    // NULL for exit which is not called that way
    int (*bi_functions[256])(BI_PROTOTYPE) = { bi_exit,  bi_true,  bi_false,
                                               bi_echo,  bi_break, bi_continue,
                                               bi_unset/*, bi_cd */};
#ifdef IMPLEMENTED
    // Just to notice that cd is not implemented
#endif
    return bi_functions[id];
}

static void free_args(char **args)
{
    if (args == NULL)
    {
        return;
    }
    size_t i = 0;
    while (args[i] != NULL)
    {
        free(args[i++]);
    }
    free(args);
}

static int get_bi_id(char *name)
{
    // return an index to an array of functions
    // or -1 if name does not match
    // Exit MUST be the first in the list
    static char bi_names[256][256] = { "exit",  "true",     "false", "echo",
                                "break", "continue", "unset", "cd" };
    int bi_nb = 8;
    int res = 0;
    while (res < bi_nb && strcmp(name, bi_names[res]))
    {
        res += 1;
    }
    if (res == bi_nb)
    {
        res = -1;
    }
    return res;
}

static int run_fun(struct ast *fun, struct context *c, char **args)
{
    char **real_argv = c->argv;
    int real_argc = c->argc;

    free(args[0]);
    args[0] = (char*)malloc(strlen(c->argv[0]) + 5);
    strcpy(args[0], c->argv[0]);
    int argc = 0;
    while (args[argc] != NULL)
    {
        argc++;
    }
    c->argc = argc;
    c->argv = args;

    exec_switch(fun, c);
    c->argv = real_argv;
    c->argc = real_argc;
    return c->last_rv;
}

void exec_argv(EXEC_PROTOTYPE)
{
    if (ast->type != AST_ARGV)
        errx(1, "exec_argv only works with AST_ARGV type");
    int exit_code = -1;
    char **args = expand((char ***)(&(ast->p1)), c);

    // user-defined function check
    struct ast *user_fun = list_func_get_value(c->list_func, args[0]);
    if (user_fun)
    {
        exit_code = run_fun(user_fun, c, args);
    }

    // builtin check
    if (exit_code == -1)
    {
        int bi_id = get_bi_id(args[0]);
        if (bi_id != -1)
        {
            exit_code = get_bi_function(bi_id)(args, c);
        }
    }

    // yeet
    if (exit_code == -1)
    {
        exit_code = run_execvp(args);
    }

    free_args(args);

    c->last_rv = exit_code;
}
