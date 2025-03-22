#ifndef ENV_VARS_H
#define ENV_VARS_H
#include "headers.h"

/**
 * Expands an environment variable. Returns an array of strings NULL terminated,
 * or just NULL if expansion failed
 * @param str variable string
 * @param c parser context
 * @return expanded string
 */
char **expand_env(const char *str, struct context *c);
#endif /* !ENV_VARS_H */
