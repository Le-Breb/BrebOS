#include "headers.h"

static size_t my_strlen(const char *s)
{
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}

static void free_arr(char **expanded)
{
    if (!expanded)
        return;
    size_t i = 0;
    while (expanded[i])
    {
        free(expanded[i++]);
    }
    free(expanded);
}

static char *compress_str(char **str_arr)
{
    if (str_arr == NULL || *str_arr == NULL)
    {
        return (char*)calloc(2, 1);
    }
    char *str_val;
    char **tmp = str_arr;
    size_t total_len = 0;
    while (*tmp)
    {
        total_len += my_strlen(*tmp++);
    }
    str_val = (char*)calloc(total_len + 1, sizeof(char));

    tmp = str_arr;
    str_val = strcat(str_val, *tmp++);
    while (*tmp)
    {
        str_val = strcat(str_val, " ");
        str_val = strcat(str_val, *tmp++);
    }
    free_arr(str_arr);
    return str_val;
}

static void get_name_and_value(const char *s, char **name, char **value)
{
    char *equal_index = strstr(s, "=");
    if (!equal_index)
        errx(1, "not a assign");

    size_t offset = equal_index - s;
    *name = (char*)calloc(offset + 1, sizeof(char));
    strncpy(*name, s, offset);

    s += offset + 1;
    size_t length = my_strlen(s);
    *value = (char*)calloc(length + 1, sizeof(char));
    strncpy(*value, s, length);
}

void exec_assign(EXEC_PROTOTYPE)
{
    char *name;
    char *value;
    get_name_and_value((const char*)ast->p1, &name, &value);
    char *ename = check_name(expand_one_s(name, c));
    char *evalue = compress_str(expand_one_s(value, c));
    free(name);
    free(value);
    c->list_var = list_add(c->list_var, ename, evalue);
    exec_switch((struct ast*)ast->p2, c);
}
