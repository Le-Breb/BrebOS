#ifndef LIST_VAR_H
#define LIST_VAR_H

struct list_var
{
    char *name;
    char *value;
    struct list_var *next;
};

// free list, names and values
void list_free(struct list_var *list_var);

// add if not exists or modify value (precedent value is free)
// return the head
struct list_var *list_add(struct list_var *head, char *name, char *value);

// free and return the head
struct list_var *list_remove(struct list_var *head, char *name);

char *list_get_value(struct list_var *list, char *name);

#endif /* ! LIST_VAR_H */
