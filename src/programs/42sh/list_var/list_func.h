#ifndef LIST_FUNC_H
#define LIST_FUNC_H

#include "ast/ast.h"

struct list_func
{
    char *name;
    struct ast *body;
    struct list_func *next;
};

void list_func_free(struct list_func *list_func);
struct list_func *list_func_add(struct list_func *head, char *name,
                                struct ast *value);
struct list_func *list_func_remove(struct list_func *head, char *name);
struct ast *list_func_get_value(struct list_func *list, char *name);

#endif /* LIST_FUNC_H */
