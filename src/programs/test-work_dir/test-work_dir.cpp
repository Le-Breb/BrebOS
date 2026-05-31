#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sys/wait.h"

// Runs cd and pwd in 42sh and checks if result is correct
int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
#define exit_err(err) {fprintf(stderr, err); return 1;}
    int pipefd[2];
    if (pipe(pipefd) == -1)
    {
        fprintf(stderr, "Pipe failed\n");
        return 1;
    }

    int c = fork();
    if (c == -1)
        exit_err("Fork failed\n")

    if (c > 0)
    {
        // Waiting is better, though not waiting widens the test by using the blocking behavior of read()
        // if (waitpid(c, nullptr, 0) == -1)
        //     exit_err("waitpid failed\n")
        close(pipefd[1]);
        constexpr size_t N_BYTES = 200;
        char buf[N_BYTES];
        if (const int r = read(pipefd[0], buf, N_BYTES - 1); r != -1)
        {
            buf[r] = '\0';
            const auto expected = "/\n";
            const bool ok = strcmp(buf, expected) == 0;
            printf("%s\n", ok ? "OK" : "FAILED");
            if (!ok)
                printf("Was expecting: '%s', got '%s'\n", expected, buf);

            return !ok;
        }

        exit_err("read failed\n")
    }
    else
    {
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
            exit_err("dup2 failed\n");
        close(pipefd[1]);

        close(pipefd[0]);
        char *argv[] = {
            (char*)"/bin/42sh",
            (char*)"-c",
            (char*)"cd /;pwd",
            nullptr
        };

        if (execvp("/bin/42sh", argv) == -1)
            exit_err("Execvp failed\n");
        __builtin_unreachable();
    }
}
