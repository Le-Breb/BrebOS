#include <kstring.h>
#include <kstdio.h>
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

void start_program()
{
	int argc = 0;
	char* argv[MAX_CMD_LEN];
	char* arg;
	char* tmp = nullptr;
    char* program_name = strtok_r(cmd + 2, " ", &tmp);
	while ((arg = strtok_r(nullptr, " ", &tmp)))
        argv[argc++] = arg;

	exec(program_name, argc, (const char**) argv);
}

void handlechar(char c)
{
	if (c == '\b')
	{
          if (!c_id)
			return;
          c_id--;
    }
	else cmd[c_id++] = c;

	printf("%c", c);
	flush();
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
	while (1)
	{
		printf(">>> ");
        flush();

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
			handlechar(c);
		}

		// P command: start a program
		if (cmd[0] == 'p' && cmd[1] == ' ')
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
        else if (cmd[0] == 'c' && cmd[1] == 'l' && cmd[2] == 'e' && cmd[3] == 'a' && cmd[4] == 'r')
          	clear_screen();
        else if (cmd[0] == 'd' && cmd[1] == 'n' && cmd[2] == 's')
        	dns(c_id >= 4 ? cmd + 4 : "www.example.com");
		else
			printf("Unknown command\n");

		clear_cmd();
	}

	return 0;
}
