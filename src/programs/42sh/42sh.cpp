#include "headers.h"

#define IO_ERROR_EXIT_CODE 2
#define GRAMMAR_ERROR_EXIT_CODE 2

int exit_42sh(int ret_val, struct lexer *lexer, struct ast *ast,
              struct context *context)
{
    if (ret_val == 2 && !context->shall_exit)
        fprintf(stderr,
                "42sh: Usage: 42sh [OPTIONS] [SCRIPT] [ARGUMENTS ...]\n");

    context_free(context);
    ast_free(ast);
    lexer_free(lexer);
    io_backend_exit();

    exit(ret_val);
}

int main(int argc, char **argv)
{
  //for (auto i = 0; i < argc; i++)
    //printf("%s\n", argv[i]);
#pragma region k_adapted
    //struct context *c = context_new(argv[0]);
    struct context *c = context_new(nullptr);
#pragma endregion
    if (io_backend_init(argc, argv, c))
        return exit_42sh(IO_ERROR_EXIT_CODE, NULL, NULL, c);

    struct lexer *lexer = lexer_new();
    enum parser_status status = PARSER_OK;

    while (status == PARSER_OK)
    {
        //printf("start parsing\n");
        struct ast *ast = parse_input(&status, lexer);
        //printf("end parsing\n");
        if (status == PARSER_UNEXPECTED_TOKEN)
        {
            fprintf(stderr, "42sh: grammar error\n");
            return exit_42sh(GRAMMAR_ERROR_EXIT_CODE, lexer, ast, c);
        }
        exec_list(ast, c);
        ast_free(ast);
    }

    return exit_42sh(c->last_rv, lexer, NULL, c);
}
