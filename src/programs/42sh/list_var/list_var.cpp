#include "headers.h"

static struct list_var *list_create(char *name, char *value)
{
    struct list_var *l = (struct list_var*)malloc(sizeof(struct list_var));
    if (!l)
        return NULL;
    l->name = name;
    l->value = value;
    l->next = NULL;
    return l;
}

static struct list_var *list_find(struct list_var *list, char *name)
{
    while (list)
    {
        if (strcmp(list->name, name) == 0)
            return list;

        list = list->next;
    }
    return list;
}

void list_free(struct list_var *list_var)
{
    while (list_var)
    {
        free(list_var->value);
        free(list_var->name);

        struct list_var *next = list_var->next;
        free(list_var);
        list_var = next;
    }
}

struct list_var *list_add(struct list_var *head, char *name, char *value)
{
    struct list_var *l = list_find(head, name);
    if (!l)
    {
        l = list_create(name, value);
        l->next = head;
        return l;
    }
    else
    {
        free(name);
        free(l->value);
        l->value = value;
        return head;
    }
}

static struct list_var *link_n_free(struct list_var *cur)
{
    struct list_var *res = cur->next;
    cur->next = NULL;
    list_free(cur);
    return res;
}

struct list_var *list_remove(struct list_var *head, char *name)
{
    if (!head)
    {
        return NULL;
    }
    if (strcmp(head->name, name) == 0)
        return link_n_free(head);

    struct list_var *prec = head;
    struct list_var *cur = head->next;
    while (cur)
    {
        if (strcmp(cur->name, name) == 0)
        {
            prec->next = link_n_free(cur);
            break;
        }
        prec = cur;
        cur = cur->next;
    }
    return head;
}

char *list_get_value(struct list_var *list, char *name)
{
    struct list_var *l = list_find(list, name);
    if (!l)
        return NULL;
    return l->value;
}
