#include "headers.h"

struct context *context_new(const char *argv0)
{
    struct context *res = (struct context*)calloc(1, sizeof(struct context));
    res->last_rv = 0;
    res->list_var = NULL;

    res->exec_path = argv0;
    return res;
}

static void free_io_argv(char **argv)
{
    if (argv == NULL)
    {
        return;
    }
    int i = 0;
    while (argv[i] != NULL)
    {
        free(argv[i++]);
    }
    free(argv);
}

void context_free(struct context *ctx)
{
    if (!ctx)
        return;
    list_free(ctx->list_var);
    list_func_free(ctx->list_func);
    free_io_argv(ctx->argv);
    free(ctx);
}
