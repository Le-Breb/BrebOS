#include <stdio.h>
#include <signal.h>

#include "unistd.h"
#include "sys/wait.h"

int parent(int);
int child();

int main() {
    const int c = fork();

    if (c == -1)
    {
        fprintf(stderr, "Fork failed");
        return 1;
    }

    if (c > 0)
        return parent(c);
    return child();
}

void handler(int sig)
{
    printf("Signal %d received\n", sig);
}

void uhandler1([[maybe_unused]] int sig)
{
    static bool first_run = true;

    if (!first_run)
    {
        sigset_t sigmask;
        sigemptyset(&sigmask);
        sigaddset(&sigmask, SIGUSR2);
        sigprocmask(SIG_BLOCK, &sigmask, nullptr);
        raise(SIGUSR2);
    }

    raise(SIGUSR2);
    printf("uhandler1\n");

    first_run = false;
}

void uhandler2([[maybe_unused]] int sig)
{
    printf("uhandler2\n");
}

/**
 * Sends signals and eventually causes self termination by sending SIGQUIT.
 * Prints expected action sequence, then perform actions
 * @return main return value
 */
int child()
{
    printf("Expected sequence: uh2, uh1, uh1, uh2, s3, 0, QUIT\n\n");

    signal(SIGUSR1, uhandler1);
    signal(SIGUSR2, uhandler2);

    raise(SIGUSR1);
    raise(SIGUSR1);

    signal(SIGQUIT, handler);
    printf("%d\n", raise(SIGQUIT));
    signal(SIGQUIT, SIG_DFL);
    printf("%d\n", raise(SIGQUIT));
    printf("This shouldn't be printed");
    return 0;
}

/**
 * Perform checks on child wstatus after performing waitpid(child_pid)
 * @param child_pid pid of child process
 * @return main return value
 */
int parent(int child_pid)
{
#define parent_err_ret(msg, ...) {fprintf(stderr, msg, __VA_ARGS__); return 1;}
#define parent_err_ret_(msg) {fprintf(stderr, msg); return 1;}
    int wstatus{};
    if (waitpid(child_pid, &wstatus, 0) == -1)
        if (child_pid == -1)
            parent_err_ret_("Fork failed")

    if (WIFEXITED(wstatus))
        parent_err_ret_("WIFEXTED returned true")

    if (!WIFSIGNALED(wstatus))
        parent_err_ret_("WIFEXTED returned false")

    if (WTERMSIG(wstatus) != SIGQUIT)
        parent_err_ret("WTERMSIG did not return SIGQUIT, but 0x%x", WTERMSIG(wstatus));

    printf("waitpid wstatus is correct\n");

    return 0;
}

