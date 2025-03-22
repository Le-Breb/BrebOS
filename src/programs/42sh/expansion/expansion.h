#ifndef EXPANSION_H
#define EXPANSION_H

#include "headers.h"

// args : adress of arglist, so we can replace the old ones
char **expand(char ***args, struct context *c);

#define EXPAND_PROTOTYPE                                                       \
    char ***final_argsp, size_t *fargs_ip, char **cur_argp, struct context *c

#define UPDATE_POINTERS(__ARGSP, __IP, __LEN)                                  \
    do                                                                         \
    {                                                                          \
        *final_argsp = (__ARGSP);                                              \
        *fargs_ip = (__IP);                                                    \
        *cur_argp += (__LEN);                                                  \
    } while (0)

// final_argsp : POINTER to final arglist (so we can realloc it)
// fargs_ip : POINTER to current position in final arglist (so we can move
// it) cur_argp : POINTER to currently processed arg (so we can move it
// forward) context
void expand_single(EXPAND_PROTOTYPE);
void expand_double(EXPAND_PROTOTYPE);
void expand_backslash(EXPAND_PROTOTYPE);

// Step 3
void expand_dollar(EXPAND_PROTOTYPE);
void expand_backtick(EXPAND_PROTOTYPE);

/*
** String Manipulation
*/
#define append_str(A, B, C) append_nstr((A), (B), (C), NULL)

/*
** Append at most `n` bytes of str` to the string in `final_argsp` at `index`.
** If `len` is NULL then append `str`.
** Move the pointer `final_argsp` after the realloc if needed.
*/
void append_nstr(char ***final_argsp, size_t index, const char *str, size_t *len);

/**
 * Expands a string. Returns an array of strings NULL terminated, or just NULL
 * if expansion failed
 * @param str variable string
 * @param c parser context
 * @return expanded string
 */
// static char **expand(const char *str, struct context *c);

char **expand_one_s(char *original, struct context *c);
char *check_name(char **expanded);
char *substitute(const char *input, const char *argv0);
#endif /* !EXPANSION_H */
