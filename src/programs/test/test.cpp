#include <stdio.h>
#include <signal.h>

void handler(int sig)
{
    printf("Signal %d received\n", sig);
}

int main() {
    signal(SIGQUIT, handler);
    printf("%d\n", raise(SIGILL));
    printf("%d\n", raise(SIGQUIT));
    signal(SIGQUIT, SIG_DFL);
    printf("%d\n", raise(SIGQUIT));
    printf("This shouldn't be printed");
    return 0;
}

