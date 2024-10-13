#include "../klib/syscalls.h"

#define MAX_CMD_LEN 10

char cmd[MAX_CMD_LEN];
unsigned int c_id = 0;

void clear_cmd()
{
	c_id = 0;
	for (int i = 0; i < MAX_CMD_LEN; ++i)
		cmd[i] = 0;
}

int p_main()
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

		if (c_id == 3 && (cmd[0] == 'p' || cmd[1] == ' '))
		{
			if (!(cmd[2] >= '0' && cmd[2] <= '9'))
				printf("Invalid program index: %c\n", cmd[2]);
			else start_process(cmd[2] - '0');
		}
		else if (c_id == 1 && cmd[0] == 'q')
			break;
		else
			printf("Unknown command\n");

		clear_cmd();
	}

	return 0;
}
