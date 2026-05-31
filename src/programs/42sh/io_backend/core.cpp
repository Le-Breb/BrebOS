#define _POSIX_C_SOURCE 200809L
#include "core.h"

#include "headers.h"
// #include "sys/cdefs.h" // Header does not exist in mlibc. Not guarded by k_adapted as include is not needed

static struct IO_ctx ctx;

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
int io_backend_init(const int argc, char *argv[], struct context *c)
{
    if (argc == 1)
    {
        ctx.file = stdin;
        c->argc = 1;
        c->argv = create_args(0, argv, 0, c);
    }
    else if (!strcmp(argv[1], "-c"))
    {
        if (argc >= 3)
        {
            ctx.file = fmemopen(argv[2], strlen(argv[2]), "r");
            c->argv = create_args(argc, argv, 1, c);
        }
        else
        {
            fprintf(stderr, "42sh: No string provided\n");
        }
    }
    else
    {
        ctx.file = fopen(argv[1], "r");
        c->argc = argc - 1;
        c->argv = create_args(0, argv + 1, 0, c);
    }
    if (ctx.file == NULL)
    {
        fprintf(stderr, "42sh: Could not process parameters\n");
        return 1;
    }
    return 0;
}

void io_backend_exit(void)
{
    if (ctx.file != NULL)
        fclose(ctx.file);
}

int read_char(void)
{
    char c;
    const ssize_t r = fread(&c, sizeof(char), 1, ctx.file);
    if (r <= 0)
        return -1; // EOF
    return c;
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
