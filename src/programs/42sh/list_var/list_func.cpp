#include "headers.h"

static struct list_func *list_func_create(char *name, struct ast *body)
{
    struct list_func *l = (struct list_func*)malloc(sizeof(struct list_func));
    if (!l)
        return NULL;
    l->name = name;
    l->body = body;
    l->next = NULL;
    return l;
}

static struct list_func *list_func_find(struct list_func *list, char *name)
{
    while (list)
    {
        if (strcmp(list->name, name) == 0)
            return list;

        list = list->next;
    }
    return list;
}

void list_func_free(struct list_func *list_func)
{
    while (list_func)
    {
        ast_free(list_func->body);
        free(list_func->name);

        struct list_func *next = list_func->next;
        free(list_func);
        list_func = next;
    }
}

struct list_func *list_func_add(struct list_func *head, char *name,
                                struct ast *body)
{
    struct list_func *l = list_func_find(head, name);
    if (!l)
    {
        l = list_func_create(name, body);
        l->next = head;
        return l;
    }
    else
    {
        free(name);
        ast_free(l->body);
        l->body = body;
        return head;
    }
}

static struct list_func *link_n_free_func(struct list_func *cur)
{
    struct list_func *res = cur->next;
    cur->next = NULL;
    list_func_free(cur);
    return res;
}

struct list_func *list_func_remove(struct list_func *head, char *name)
{
    if (!head)
    {
        return NULL;
    }
    if (strcmp(head->name, name) == 0)
        return link_n_free_func(head);

    struct list_func *prec = head;
    struct list_func *cur = head->next;
    while (cur)
    {
        if (strcmp(cur->name, name) == 0)
        {
            prec->next = link_n_free_func(cur);
            break;
        }
        prec = cur;
        cur = cur->next;
    }
    return head;
}

struct ast *list_func_get_value(struct list_func *list, char *name)
{
    struct list_func *l = list_func_find(list, name);
    if (!l)
        return NULL;
    return l->body;
}
