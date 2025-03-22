#ifndef TOKEN_H
#define TOKEN_H

#define SEPARATOR " \t\v\f\r&|<>;\n"

enum token_type
{
    TOKEN_AND,
    TOKEN_OR,

    TOKEN_REDIR_LG,
    TOKEN_REDIR_GO,
    TOKEN_REDIR_LA,
    TOKEN_REDIR_GA,
    TOKEN_REDIR_GG,
    TOKEN_REDIR_G,
    TOKEN_REDIR_L,

    TOKEN_NOT,
    TOKEN_PIPE,

    TOKEN_SEMICOLON,
    TOKEN_NEWLINE,
    TOKEN_QUOTE,
    TOKEN_EOF, // end of input marker

    TOKEN_IONUMBER,

    // Keyword
    TOKEN_IF,
    TOKEN_THEN,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_FI,

    TOKEN_WHILE,
    TOKEN_UNTIL,
    TOKEN_DO,
    TOKEN_DONE,
    TOKEN_FOR,

    TOKEN_CASE,
    TOKEN_IN,
    TOKEN_ESAC,

    TOKEN_RBRACE, // }
    TOKEN_LBRACE, // {

    TOKEN_RPAREN, // )
    TOKEN_LPAREN, // (

    TOKEN_WORD,
    TOKEN_ASSIGNMENT_WORD,
    TOKEN_ERROR, // it is not a real token, it is returned in case of invalid
                 // input
    TOKEN_EMPTY
};

struct token_model
{
    const char *str;
    enum token_type type;
};

struct token
{
    enum token_type type; // The kind of token
    char *value; // If the token is a word, its string
};

#endif /* !TOKEN_H */
