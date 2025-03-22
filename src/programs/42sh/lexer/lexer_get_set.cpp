#include "headers.h"
#include "lexer.h"
#include "token.h"

char *get_next_str(struct lexer *lexer);

void set_lexer_status(struct lexer *lexer, char c)
{
    switch (c)
    {
    case '\'':
        lexer->status = SINGLE_QUOTE;
        break;
    case '\"':
        lexer->status = DOUBLE_QUOTE;
        break;
    case '`':
        lexer->status = BACKTICK;
        break;
    default:
        break;
    }
}

static bool match_keyword(char *next_str, struct token *tok)
{
    static struct token_model keywords[] = {
        { "if", TOKEN_IF },       { "then", TOKEN_THEN },
        { "elif", TOKEN_ELIF },   { "else", TOKEN_ELSE },
        { "fi", TOKEN_FI },       { "while", TOKEN_WHILE },
        { "do", TOKEN_DO },       { "done", TOKEN_DONE },
        { "until", TOKEN_UNTIL }, { "for", TOKEN_FOR },
        { "in", TOKEN_IN },       { "while", TOKEN_WHILE }
    };

    for (size_t i = 0; i < sizeof(keywords) / sizeof(*keywords); ++i)
    {
        if (!strcmp(next_str, keywords[i].str))
        {
            tok->type = keywords[i].type;
            return true;
        }
    }
    return false;
}

static char match_separator(char *next_str, struct token *tok)
{
    static struct token_model always_check[] = {
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

    for (size_t i = 0; i < sizeof(always_check) / sizeof(*always_check); ++i)
    {
        if (!strcmp(next_str, always_check[i].str))
        {
            tok->type = always_check[i].type;
            return true;
        }
    }
    return false;
}

static char is_IO_NUMBER(struct lexer *lexer, char *next_str)
{
    size_t size = 0;
    while (next_str[size] && isdigit(next_str[size]))
        size++;

    return next_str[size] == '\0'
        && (lexer->input[lexer->pos + size] == '<'
            || lexer->input[lexer->pos + size] == '>');
}

static char change_token_type(struct lexer *lexer, struct token *token,
                              char *next_str)
{
    switch (lexer->status)
    {
    case ERROR:
        token->type = TOKEN_ERROR;
    /* FALLTHROUGH */
    case SINGLE_QUOTE:
    case DOUBLE_QUOTE:
        return 1;
    default:
        if (is_IO_NUMBER(lexer, next_str))
        {
            token->type = TOKEN_IONUMBER;
            return 1;
        }
        return match_keyword(next_str, token);
    }
}

struct token lexer_next_token(struct lexer *lexer, size_t *len)
{
    char *next_str = get_next_str(lexer);
    struct token tok = { TOKEN_WORD, next_str };

    if (next_str == NULL)
    {
        tok.type = TOKEN_EOF;
        return tok;
    }

    if (change_token_type(lexer, &tok, next_str))
        goto leave;

    match_separator(next_str, &tok);

leave:
    lexer->status = NONE;
    *len = strlen(tok.value);
    if (tok.type != TOKEN_WORD && tok.type != TOKEN_IONUMBER)
        free(next_str);

    return tok; // Word
}
