#include "unistd.h"
#include "sys/wait.h"
#include <signal.h>
#include <stdio.h>

/**
 * Initial program executed by the kernel.
 * It fulfills two goals:
 *  - start the terminal
 *  - reap orphan processes
 */

void on_child_terminated([[maybe_unused]] int sig)
{
    int wstatus;
    while (waitpid(-1, &wstatus, WNOHANG) > 0) {}
}

int start_terminal()
{
    char* const argv[2] = {(char*)"/bin/terminal", nullptr};
    char* const envp[4] = {
        (char*)"MLIBC_DEBUG_PRINTF=0", (char*)"MLIBC_DEBUG_MALLOC=0", (char*)"NOPE_MLIBC_RTLD_DEBUG=1", nullptr
    };
    execve("/bin/terminal", argv, envp);

    fprintf(stderr, "init execve to start terminal failed");
    return 1;
}

int main()
{
    signal(SIGCHLD, on_child_terminated);

    if (const int c = fork(); c == -1)
    {
        fprintf(stderr, "init fork failed");
        return 1;
    }
    else if (c == 0)
        return start_terminal();

    // ReSharper disable once CppDFAEndlessLoop
    while (true)
    {
        sleep(1);
    }
}
