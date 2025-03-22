#ifndef BUILTINS_H
#define BUILTINS_H

#include "headers.h"

struct context;

#define UNUSED(x) (void)(x)
#define BI_PROTOTYPE char **args, struct context *c

int bi_true(BI_PROTOTYPE);
int bi_false(BI_PROTOTYPE);
int bi_echo(BI_PROTOTYPE);
int bi_exit(BI_PROTOTYPE);
int bi_break(BI_PROTOTYPE);
int bi_continue(BI_PROTOTYPE);
int bi_unset(BI_PROTOTYPE);
int bi_cd(BI_PROTOTYPE);

#endif /* ! BUILTINS_H */
