#include <kstring.h>
#include <kstdio.h>
#include <ksyscalls.h>
#include <kunistd.h>

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

		int argc = 2;
		const char* argv[2] = { "-c", cmd };
		exec("42sh", argc, (const char**) argv);

		clear_cmd();
	}

	return 0;
}
