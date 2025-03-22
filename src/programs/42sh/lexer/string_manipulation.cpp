#define _POSIX_C_SOURCE 200809L

#include "headers.h"

#define SEPARATORS "&|<>;\n()"
#define WHITESPACE " \t\r"
#define OPERATORS "|&<>"

char is_valid_operator(char *str)
{
    static struct token_model valid_ope[] = {
        { "|", TOKEN_PIPE },      { "!", TOKEN_NOT },
        { ";", TOKEN_SEMICOLON }, { "\n", TOKEN_NEWLINE },
        { "\0", TOKEN_EOF },      { ">", TOKEN_REDIR_G },
        { "<", TOKEN_REDIR_L },   { ">>", TOKEN_REDIR_GG },
        { ">&", TOKEN_REDIR_GA }, { "<&", TOKEN_REDIR_LA },
        { ">|", TOKEN_REDIR_GO }, { "<>", TOKEN_REDIR_LG },
        { "&&", TOKEN_AND },      { "||", TOKEN_OR },
        { "(", TOKEN_LPAREN },    { ")", TOKEN_RPAREN },
        { "{", TOKEN_LBRACE },    { "}", TOKEN_RBRACE },
    };

    for (size_t i = 0; i < sizeof(valid_ope) / sizeof(*valid_ope); ++i)
        if (!strcmp(str, valid_ope[i].str))
            return 1;

    return 0;
}

static size_t skip_whitespaces_and_comments(struct lexer *lexer)
{
    const char *input = lexer->input + lexer->pos;
    size_t size = 0;

    // Skip whitespaces
    auto rr = strspn(input, WHITESPACE);
    size += rr;

    // Skip comments
    input += size;
    if (*input == '#')
        size += strcspn(input, "\n");
    lexer->pos += size;
    return size;
}

static int is_separator(char c)
{
    return strchr(SEPARATORS, c) != NULL;
}

static int is_whitespace(char c)
{
    return strchr(WHITESPACE, c) != NULL;
}

static int is_redirection(char c)
{
    return strchr(OPERATORS, c) != NULL;
}

static char get_next_line(struct lexer *lexer, const char **input, size_t *pos)
{
    char *new_input = strdup(lexer->input);
    char *next_line = io_backend_get_line();

    if (!next_line)
    {
        free(new_input);
        return 0;
    }

    new_input = (char*)realloc(new_input, strlen(new_input) + strlen(next_line) + 1);
    new_input = strcat(new_input, next_line);

    free(next_line);
    free((void *)lexer->input);
    lexer->input = new_input;

    *input = new_input + lexer->pos + ++(*pos);

    return 1;
}

static char refresh_input(struct lexer *lexer, const char **input)
{
    if (*input && **input == '\0')
    {
        free((void *)lexer->input);
        lexer->input = io_backend_get_line();
        //printf("iinput: %s\n", lexer->input ? lexer->input : "no input");
        lexer->pos = 0;
        *input = lexer->input;
        if (!*input)
            return 0;
        *input += skip_whitespaces_and_comments(lexer);
    }
    return 1;
}

static void is_valid_ope(struct lexer *lexer, const char **__input,
                         size_t *__size)
{
    const char *start = *__input + 1;
    size_t size = *__size + 1;

    while (*start && is_redirection(*start))
    {
        size++;
        start++;
    }

    char *__to_free = strndup(*__input, size);
    if (is_valid_operator(__to_free))
        goto valid;

    lexer->status = ERROR;

valid:
    free(__to_free);
    *__size = size;
    *__input = start;
}

static void read_until_char(struct lexer *lexer, const char **__input,
                            size_t *__size, char c)
{
    const char *start = *__input + 1;
    size_t size = *__size + 1;

    while (*start)
    {
        if (*start == c)
        {
            *__size = size + 1;
            *__input = start + 1;
            return;
        }
        else if (*start == '\\')
        {
            size++;
            start++;
            if (*start == '\'' && *start == c)
            {
                *__size = size + 1;
                *__input = start + 1;
                return;
            }
            else if (*start == '\n')
            {
                if (!get_next_line(lexer, &start, &size))
                    break;
                continue;
            }
        }
        else if (*start == '\'' || *start == '"' || *start == '`')
        {
            if (c == ')')
            {
                read_until_char(lexer, &start, &size, *start);
                continue;
            }
        }
        else if (*start == '\n')
        {
            if (!get_next_line(lexer, &start, &size))
                break;
            continue;
        }
        size++;
        start++;
    }
    lexer->status = ERROR;
}

static char *read_until_separator(struct lexer *lexer, const char **input)
{
    const char *start = *input;
    size_t size = 0;

    while (*start && !is_separator(*start) && lexer->status != ERROR)
    {
        if (is_whitespace(*start))
        {
            break;
        }
        else if (*start == '$')
        {
            size++;
            start++;
            if (*start == '(')
            {
                read_until_char(lexer, &start, &size, ')');
            }
        }
        else if (*start == '\\')
        {
            size++;
            start++;
            if (*start == '\n')
            {
                if (!get_next_line(lexer, &start, &size))
                    return NULL;
                continue;
            }
            else if (*start)
            {
                size++;
                start++;
            }
        }
        else if (*start == '\'' || *start == '"' || *start == '`')
        {
            read_until_char(lexer, &start, &size, *start);
        }
        else
        {
            start++;
            size++;
        }
    }

    if (size == 0 && is_separator(*start))
    {
        is_valid_ope(lexer, &start, &size);
    }

    *input = lexer->input + lexer->pos;
    auto dup = strndup(*input, size);
    return dup;
}

char *get_next_str(struct lexer *lexer)
{
    if (!lexer->input)
        return NULL;

    skip_whitespaces_and_comments(lexer);
    //printf("input: %s\n", lexer->input);
    const char *input = lexer->input + lexer->pos;
    //printf("input: %s | %zu\n", lexer->input + lexer->pos, lexer->pos);

    if (!refresh_input(lexer, &input))
        return NULL;
    //printf("input: %s\n", lexer->input ? lexer->input : "no input");

    return read_until_separator(lexer, &input);
}
