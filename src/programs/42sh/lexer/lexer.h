#ifndef LEXER_H
#define LEXER_H

#include "token.h"

enum lexer_status
{
    SINGLE_QUOTE,
    DOUBLE_QUOTE,
    BACKTICK,
    NONE,
    ERROR
};

struct lexer
{
    const char *input; // The input data
    size_t pos; // The current offset inside the input data
    enum lexer_status status; // State being processed
    struct token next;
    struct token sur_next;
};

/**
 * \brief Returns new lexer.
 *
 * Set the input with the next line read from the input.
 */
struct lexer *lexer_new(void);

/**
 * \brief Returns a copy of the lexer.
 *
 * The `input` string (a `const char *`) associated with the lexer is also
 * copied into a newly allocated buffer using `malloc`.
 */
struct lexer *lexer_make_save(struct lexer *lexer);

/**
 * \brief Set the state from `lexer_save` to `lexer`.
 *
 * The `input` is duplicated from `lexer_save`.
 * This functions calls `lexer_free` on `lexer_save`.
 */
void lexer_rollback(struct lexer *lexer, struct lexer *lexer_save);

/**
 ** \brief Frees the given lexer, but not its input.
 */
void lexer_free(struct lexer *lexer);

/**
 * \brief Returns the next token, but doesn't move forward: calling lexer_peek
 * multiple times in a row always returns the same result.
 * This function is meant to help the parser check if the next token matches
 * some rule.
 */
struct token lexer_peek(struct lexer *lexer);

struct token lexer_peek_2(struct lexer *lexer);

/**
 * \brief Returns the next token, and removes it from the stream:
 *   calling lexer_pop in a loop will iterate over all tokens until EOF.
 */
struct token lexer_pop(struct lexer *lexer);

struct lexer *lexer_make_save(struct lexer *lexer);

void lexer_rollback(struct lexer *lexer, struct lexer *lexer_save);

#endif /* !LEXER_H */
