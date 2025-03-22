#ifdef IMPLEMENTED

#include "headers.h"

static int dup_fd(int fd)
{
    int d = dup(fd);
    if (d == -1)
        errx(1, "dup failed");
    return d;
}

static void dup2_(int oldfd, int newfd)
{
    if (dup2(oldfd, newfd) == -1)
        errx(1, "dup2 failed");
}

static size_t get_number_of_pipes(const struct ast *pipe)
{
    size_t size = 0;
    for (; pipe; pipe = pipe->p2)
        size++;
    if (size == 0)
        errx(1, "No pipes found in get_number_of_pipes");
    return size - 1;
}

static void pipe_init(EXEC_PROTOTYPE)
{
    size_t const number_of_pipes = get_number_of_pipes(ast);
    // printf("%zu\n", number_of_pipes);
    fflush(stdout);
    c->pipe_index = 0;
    c->number_of_pipes = number_of_pipes;
    c->input_pipe_set = 0;
    for (size_t i = 0; i < 2; i++)
    {
        if (pipe(c->pipes[i]) != 0)
            errx(1, "pipe failed in pipe_init");
    }
}

static void pipe_cleanup(EXEC_PROTOTYPE)
{
    UNUSED(ast);
    close(c->pipes[0][0]);
    close(c->pipes[0][1]);
    close(c->pipes[1][0]);
    close(c->pipes[1][1]);
    c->number_of_pipes = 0; // Will trigger pipe init on next pipe execution
}

static void restore_stdout(EXEC_PROTOTYPE)
{
    UNUSED(ast);
    if (c->stdout_save == -1)
        errx(1, "stdout_save not set");

    dup2_(c->stdout_save, STDOUT_FILENO);
    close(c->stdout_save);
    c->stdout_save = -1;
}

static void restore_stdin(EXEC_PROTOTYPE)
{
    UNUSED(ast);
    if (c->stdin_save == -1)
        errx(1, "stdin_save not set");

    dup2_(c->stdin_save, STDIN_FILENO);
    close(c->stdin_save);
    c->stdin_save = -1;
}

static void save_stdin(EXEC_PROTOTYPE)
{
    UNUSED(ast);
    c->stdin_save = dup_fd(STDIN_FILENO);
}

static void save_stdout(EXEC_PROTOTYPE)
{
    UNUSED(ast);
    c->stdout_save = dup_fd(STDOUT_FILENO);
}

static void exec_first_pipe(EXEC_PROTOTYPE)
{
    save_stdout(ast, c);
    dup2_(c->pipes[0][1], STDOUT_FILENO);
    c->pipe_index++;
    exec_switch(ast->p1, c);
    close(c->pipes[0][1]); // Send EOF
    restore_stdout(ast, c);
    exec_switch(ast->p2, c);
}

static void exec_middle_pipe(EXEC_PROTOTYPE)
{
    int input_pipe_set = (c->pipe_index - 1) % 2;
    int output_pipe_set = (input_pipe_set + 1) % 2;

    save_stdin(ast, c);
    save_stdout(ast, c);

    dup2_(c->pipes[input_pipe_set][0], STDIN_FILENO);
    dup2_(c->pipes[output_pipe_set][1], STDOUT_FILENO);

    c->pipe_index++;
    exec_switch(ast->p1, c);
    restore_stdin(ast, c);
    restore_stdout(ast, c);
    close(c->pipes[input_pipe_set][0]); // Close read fd
    close(c->pipes[output_pipe_set][1]); // Send EOF
    if (pipe(c->pipes[input_pipe_set])) // Open another pipe for the future
        errx(1, "pipe failed in exec_pipe");
    exec_switch(ast->p2, c);
}

static void exec_last_pipe(EXEC_PROTOTYPE)
{
    int input_pipe_set = (c->pipe_index - 1) % 2;

    save_stdin(ast, c);
    dup2_(c->pipes[input_pipe_set][0], STDIN_FILENO);

    exec_switch(ast->p1, c);
    restore_stdin(ast, c);
    exec_switch(ast->p2, c);

    // Close everything
    close(c->pipes[0][0]);
    close(c->pipes[1][0]);
    close(c->pipes[0][1]);
    close(c->pipes[1][1]);
}

void exec_pipe(EXEC_PROTOTYPE)
{
    if (!c->number_of_pipes)
    {
        pipe_init(ast, c);
        exec_first_pipe(ast, c);
        return;
    }
    if (c->pipe_index < c->number_of_pipes)
    {
        exec_middle_pipe(ast, c);
        return;
    }

    exec_last_pipe(ast, c);
    pipe_cleanup(ast, c);
}

#endif