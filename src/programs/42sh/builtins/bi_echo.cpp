#include "headers.h"

static char **get_options(char **args, char *nonewline, char *noescape)
{
    while (*args != NULL && *args[0] == '-')
    {
        char nnl = *nonewline;
        char ne = *noescape;
        char *cur = *args + 1;
        char stop = 0;
        while (*cur != 0 && !stop)
        {
            switch (*cur)
            {
            case 'e':
                ne = 0;
                break;
            case 'E':
                ne = 1;
                break;
            case 'n':
                nnl = 1;
                break;
            default:
                stop = 1;
                break;
            }
            cur++;
        }
        if (!stop)
        {
            *nonewline = nnl;
            *noescape = ne;
            args++;
        }
        else
        {
            break;
        }
    }
    return args;
}

static char print_s(char *string, char noescape)
{
    while (*string != 0)
    {
        if (!noescape && *string == '\\')
        {
            if (string[1] == 'n')
            {
                putchar(10);
                string++;
            }
            else if (string[1] == 't')
            {
                putchar(9);
                string++;
            }
            else if (string[1] == '\\')
            {
                putchar('\\');
                string++;
            }
            else
            {
                putchar('\\');
            }
        }
        else
        {
            putchar(*string);
        }
        string++;
    }
    return 1;
}

int bi_echo(BI_PROTOTYPE)
{
    UNUSED(c);
    char nonewline = 0;
    char noescape = 1;
    char space = 0;
    char **toprint = get_options(args + 1, &nonewline, &noescape);
    while (*toprint != NULL)
    {
        if (space)
        {
            printf(" ");
        }
        space = print_s(*toprint, noescape);
        toprint++;
    }
    if (!nonewline)
    {
        printf("\n");
    }
#pragma region k_adapted
    // fflush(stdout);
    flush();
#pragma endregion
    return 0;
}
