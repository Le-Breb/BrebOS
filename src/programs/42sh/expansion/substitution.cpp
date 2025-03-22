#ifdef IMPLEMENTED

#include "../headers.h"

char *read_output(const int pipefd[])
{
    const size_t read_step = 1024;
    size_t buffer_size = read_step;
    char *substitution = (char*)malloc(buffer_size);
    size_t total_read = 0;

    ssize_t nread;
    while ((nread = read(pipefd[0], substitution + total_read, read_step)) > 0)
    {
        total_read += nread;
        if ((size_t)nread == read_step)
        {
            buffer_size += read_step;
            substitution = realloc(substitution, buffer_size);
        }
    }

    if (nread == -1)
        err(1, "read failed");

    close(pipefd[0]);
    substitution[total_read] = '\0'; // Null-terminate
    size_t p = total_read - 1;
    while (p < total_read && substitution[p] == '\n')
        substitution[p--] = '\0';
    return substitution;
}

char *substitute(const char *input, const char *argv0)
{
    int pipefd[2];
    if (-1 == pipe(pipefd))
        errx(1, "pipe failed");

    pid_t pid = fork();
    if (pid < 0)
        errx(1, "fork failed");

    if (pid == 0)
    { // child
        char *const argv[] = { (char *)argv0, "-c", (char *)input, NULL };
        close(pipefd[0]);
        if (-1 == dup2(pipefd[1], STDOUT_FILENO))
            errx(1, "dup2 failed");
        close(pipefd[1]);
        if (-1 == execvp(argv0, argv))
            errx(1, "execvp failed");
    }

    int status;
    waitpid(pid, &status, 0);
    close(pipefd[1]);

    return read_output(pipefd);
}
#endif