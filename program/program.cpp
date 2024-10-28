#include "../klib/syscalls.h"

#define MAX_CMD_LEN 100

char cmd[MAX_CMD_LEN];
unsigned int c_id = 0;

void clear_cmd()
{
	c_id = 0;
	for (int i = 0; i < MAX_CMD_LEN; ++i)
		cmd[i] = 0;
}

int main()
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
		if (c_id == 3 && cmd[0] == 'p' && cmd[1] == ' ')
		{
			if (!(cmd[2] >= '0' && cmd[2] <= '9'))
				printf("Invalid program index: %c\n", cmd[2]);
			else start_process(cmd[2] - '0');
		}
			// Q command: shutdown
		else if (c_id == 1 && cmd[0] == 'q')
			shutdown();
		else if (cmd[0] == 'm' && cmd[1] == ' ' && (cmd[2] >= '0' && cmd[2] <= '9') && cmd[3] == ' ')
			printf("Creation of folder %s: %s\n", cmd + 4, mkdir(cmd[2] - '0', cmd + 4) ? "success" : "failed");
		else if (cmd[0] == 't' && cmd[1] == ' ' && (cmd[2] >= '0' && cmd[2] <= '9') && cmd[3] == ' ')
			printf("Creation of file %s: %s\n", cmd + 4, touch(cmd[2] - '0', cmd + 4) ? "success" : "failed");
		else if (cmd[0] == 'l' && cmd[1] == ' ' && (cmd[2] >= '0' && cmd[2] <= '9') && cmd[3] == ' ')
			printf("Ls at %s: %s\n", cmd + 4, ls(cmd[2] - '0', cmd + 4) ? "success" : "failed");
		else
			printf("Unknown command\n");

		clear_cmd();
	}

	return 0;
}
