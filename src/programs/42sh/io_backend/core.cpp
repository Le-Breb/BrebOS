#define _POSIX_C_SOURCE 200809L
#include "headers.h"

#pragma region k_adapted
//static struct IO_ctx ctx;
#pragma endregion

static char **create_args(const int argc, char **argv, char type,
                          struct context *c)
{
    if (type == 0 || argc > 3)
    {
        if (type == 1)
        {
            c->argc = argc - 3;
            argv = argv + 3;
        }
        char **res = (char**)malloc((argc + 5) * sizeof(char *));
        int i = 0;
        while (argv[i] != NULL)
        {
            res[i] = (char*)malloc(strlen(argv[i]) + 5);
            strcpy(res[i], argv[i]);
            i++;
        }
        res[i] = NULL;
        return res;
    }
    else
    {
        char **res = (char**)malloc(2 * sizeof(char *));
        c->argc = 1;
        res[0] = (char*)malloc(strlen(argv[0]) + 5);
        strcpy(res[0], argv[0]);
        res[1] = NULL;
        return res;
    }
}
static const char* chars;
int io_backend_init(const int argc, char *argv[], struct context *c)
{
#ifdef IMPLEMENTED
    if (argc == 1)
    {
        ctx.file = stdin;
        c->argc = 1;
        c->argv = create_args(0, argv, 0, c);
    }
    else
#endif
#pragma region k_adapted
    // if (!strcmp(argv[1], "-c"))
    if (argc > 1 && !strcmp(argv[1], "-c"))
#pragma endregion
    {
        if (argc >= 3)
        {
#pragma region k_adapted
            //ctx.file = fmemopen(argv[2], strlen(argv[2]), "r");
            chars = argv[2];
#pragma endregion
            c->argv = create_args(argc, argv, 1, c);
        }
        else
        {
            fprintf(stderr, "42sh: No string provided\n");
            return 1;
        }
    }
#ifdef IMPLEMENTED
    else
    {
        ctx.file = fopen(argv[1], "r");
        c->argc = argc - 1;
        c->argv = create_args(0, argv + 1, 0, c);
    }
#endif
#pragma region k_adapted
    //if (ctx.file == NULL)
    if (argc == 1 || strcmp(argv[1], "-c")) // pragma region breaks access to previous if so i had to write the opposite condition
    {
        fprintf(stderr, "42sh: Could not process parameters\n");
        return 1;
    }
#pragma endregion
    return 0;
}

void io_backend_exit(void)
{
#ifdef IMPLEMENTED
    if (ctx.file != NULL)
        fclose(ctx.file);
#endif
}

int read_char(void)
{
#pragma region k_adapted
    //char c;
    //const ssize_t r = fread(&c, sizeof(char), 1, ctx.file);
    //if (r <= 0)
    //    return -1; // EOF
    //return c;
    return *chars ? *chars++ : -1;
#pragma endregion
}

char *io_backend_get_line(void)
{
    int buf_capacity = 1024;
    int buf_size = 0;

    int r = read_char();
    if (r == -1)
    {
        return NULL;
    }
    char *buf = (char*)malloc(buf_capacity);
    while (r > 0 && r != '\n')
    {
        buf[buf_size++] = r;
        if (buf_size > buf_capacity - 3)
        {
            buf_capacity *= 2;
            buf = (char*)realloc(buf, buf_capacity);
        }
        r = read_char();
    }
    if (r == '\n')
    {
        buf[buf_size++] = '\n';
    }
    buf[buf_size] = 0;
    if (r == -2)
    {
        free(buf);
        return NULL;
    }

    return buf;
}
