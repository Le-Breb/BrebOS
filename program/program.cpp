#include "../klib/syscalls.h"

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

void start_program()
{
	if (!DIGIT(cmd[2]))
		printf("Invalid program index: %c\n", cmd[2]);
	else
	{
		int argc = 0;
		char* argv[MAX_CMD_LEN];
		char* beg = &cmd[4]; // Beginning of current arg
		for (unsigned int i = 4; i < c_id; ++i)
		{
			if (cmd[i] == ' ') // Enf of current arg
			{
				cmd[i] = '\0'; // Terminate string
				argv[argc++] = beg; // Add ptr to argv and update argc
				beg = &cmd[i + 1]; // Update beg to point to next arg
			}
		}
		if (c_id != 3)
		{
			cmd[c_id] = '\0'; // Terminate final arg
			argv[argc++] = beg; // Add ptr to argv and update argc
		}

		start_process(cmd[2] - '0', argc, (const char**) argv);
	}
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	while (1)
	{
		printf(">>> ");

		while (1)
		{
			if (c_id == MAX_CMD_LEN)
			{
				printf("Command should be less than %u characters long\n", MAX_CMD_LEN);
				break;
			}

			char c = get_keystroke();

			if (c == '\n')
			{
				printf("\n");
				break;
			}

			cmd[c_id++] = c;
			printf("%c", c);
		}

		// P command: start a program
		if (cmd[0] == 'p' && cmd[1] == ' ' && (c_id == 3 || cmd[3] == ' '))
			start_program();
			// Q command: shutdown
		else if (c_id == 1 && cmd[0] == 'q')
			shutdown();
		else if (cmd[0] == 'm' && cmd[1] == 'k' && cmd[2] == 'd' && cmd[3] == 'i' && cmd[4] == 'r' && cmd[5] == ' ')
			mkdir(cmd + 6);
		else if (cmd[0] == 't' && cmd[1] == 'o' && cmd[2] == 'u' && cmd[3] == 'c' && cmd[4] == 'h' && cmd[5] == ' ')
			touch(cmd + 6);
		else if (cmd[0] == 'l' && cmd[1] == 's' && cmd[2] == ' ')
			ls(cmd + 3);
		else
			printf("Unknown command\n");

		clear_cmd();
	}

	return 0;
}
