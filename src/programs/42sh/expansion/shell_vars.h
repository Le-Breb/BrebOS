#ifndef SHELL_VARS_H
#define SHELL_VARS_H

#include "headers.h"

/**
 * Expands a shell variable. Returns an array of strings NULL terminated, or
 * just NULL if expansion failed
 * @param str variable string
 * @param c parser context
 * @return expanded string
 */
char **expand_var(const char *str, struct context *c);

#endif /* !SHELL_VARS_H */
