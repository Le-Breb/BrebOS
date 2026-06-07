#include <stdio.h>
#include <signal.h>

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

int main() {
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

