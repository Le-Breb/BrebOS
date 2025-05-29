#include "headers.h"

void exec_subshell(EXEC_PROTOTYPE)
{
    pid_t pid = fork();
    if (pid < 0)
        errx(1, "fork failed");

    if (pid == 0) // child
    {
        exec_switch((struct ast*)ast->p1, c);
        exit(c->last_rv);
    }
    // parent
    int status;
    waitpid(pid, &status, 0);
    int exit_status = WEXITSTATUS(status);
    c->last_rv = exit_status;
}