#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ksyscalls.h>

#define MAX_CMD_LEN 100

#define DIGIT(c) (c >= '0' && c <= '9')

char cmd[MAX_CMD_LEN];
unsigned int c_id = 0;

void clear_cmd()
{
	c_id = 0;
	for (int i = 0; i < MAX_CMD_LEN; ++i)
		cmd[i] = 0;
}

void handlechar(char c)
{
	if (c == '\b')
	{
        if (!c_id)
			return;
		cmd[--c_id] = '\0';
    }
	else cmd[c_id++] = c;

	printf("%c", c);
	fflush(stdout);
}

void exec(const char** argv)
{
	pid_t child_pid = fork();
	if (child_pid == -1)
	{
		fprintf(stderr, "Fork failed\n");
		_exit(1);
	}
	if (child_pid == 0)
	{
		if (execve(argv[0], (char**)argv, 0) == -1)
		{
			fprintf(stderr, "Execve failed\n");
			_exit(1);
		}

		__builtin_unreachable();
	}
	if (waitpid(child_pid, nullptr, 0) == -1)
	{
		fprintf(stderr, "Waitpid failed\n");
		_exit(1);
	}
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	while (1)
	{
		printf(">>> ");
        fflush(stdout);

		while (1)
		{
			if (c_id == MAX_CMD_LEN)
			{
				fprintf(stderr, "Command should be less than %u characters long\n", MAX_CMD_LEN);
				break;
			}

			char c = get_keystroke();

			if (c == '\n')
			{
				putchar('\n');
				break;
			}
			handlechar(c);
		}

		if (c_id != MAX_CMD_LEN)
		{
			const char* argv[4] = {"42sh", "-c", cmd, nullptr};
			exec(argv);
		}

		clear_cmd();
	}

	return 0;
}
