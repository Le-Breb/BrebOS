#ifdef IMPLEMENTED
#include "headers.h"

#define PATH_MAX 4096 // Including null-byte

#define CLEAN(__ARG, __CURPATH, __PATH, __CUR_DIR, __BOOL)                     \
    do                                                                         \
    {                                                                          \
        if (__BOOL)                                                            \
        {                                                                      \
            fprintf(stderr, "cd: %s: ", (__ARG));                              \
            perror("");                                                        \
        }                                                                      \
        free((__CUR_DIR));                                                     \
        free((__CURPATH));                                                     \
        free((__PATH));                                                        \
    } while (0);                                                               \
    return (__BOOL);

static void update_pwd(char *new_pwd, char *cur_pwd)
{
    char *__pwd = getenv("PWD");
    if (__pwd == NULL)
    {
        setenv("OLDPWD", cur_pwd, 1);
    }
    else
    {
        setenv("OLDPWD", __pwd, 1);
    }
    setenv("PWD", new_pwd, 1);
}

static char step_1(BI_PROTOTYPE)
{
    UNUSED(c);
    char *HOME = getenv("HOME");
    if (*args == NULL && !HOME)
        return 0;
    return 1;
}

static void step_2(BI_PROTOTYPE)
{
    UNUSED(c);
    char *HOME = getenv("HOME");
    if (*args == NULL && HOME)
        *args = strdup(HOME);
}

static char step_3(BI_PROTOTYPE, char **curpath)
{
    UNUSED(c);
    if (**args != '/')
        return 0;

    strcat(*curpath, *args);
    return 1;
}

static void step_6(char **arg, char **curpath)
{
    *curpath = strcpy(*curpath, *arg);
}

static void step_7(char **curpath, char **args, char **arg)
{
    if (*(args + 1) == NULL)
        free(*arg);

    if (**curpath == '/')
        return;

    char *pwd = getenv("PWD");
    if (!pwd)
        return;

    char *new_curpath = calloc(PATH_MAX, sizeof(char));
    new_curpath = getcwd(new_curpath, PATH_MAX);
    size_t len = strlen(pwd);
    if (len > 0 && pwd[len - 1] != '/')
        strcat(new_curpath, "/");
    strcat(new_curpath, *curpath);
    free(*curpath);
    *curpath = new_curpath;
}

static void canonicalize_curpath(char *stack[PATH_MAX], size_t stack_size,
                                 char **path, char *curpath)
{
    if (*curpath == '/')
        strcat(*path, "/");

    for (size_t i = 0; i < stack_size; i++)
    {
        strcat(*path, stack[i]);
        if (i < stack_size - 1)
        {
            strcat(*path, "/");
        }
    }

    size_t len = strlen(*path);
    if (len > 1 && (*path)[len - 1] == '/')
    {
        (*path)[len - 1] = '\0';
    }
}

static char is_valid_dir(char *stack[PATH_MAX], size_t stack_size,
                         char *curpath)
{
    char *path = calloc(PATH_MAX, sizeof(char));
    canonicalize_curpath(stack, stack_size, &path, curpath);

    struct stat buf;
    if (stat(path, &buf) != -1 && S_ISDIR(buf.st_mode))
    {
        free(path);
        return 1;
    }
    free(path);
    return 0;
}

static char *canonicalize_path(char *curpath)
{
    if (!curpath || strlen(curpath) == 0)
        return NULL;

    char *path = calloc(PATH_MAX, sizeof(char));
    char *token;
    char *stack[PATH_MAX];
    size_t stack_size = 0;

    char *copy = strdup(curpath);
    token = strtok(copy, "/");

    while (token)
    {
        if (!strcmp(token, "."))
        {
            // Skip '.'
        }
        else if (!strcmp(token, ".."))
        {
            if (stack_size > 0 && strcmp(stack[stack_size - 1], ".."))
            {
                if (!is_valid_dir(stack, stack_size, curpath))
                {
                    free(path);
                    return NULL;
                }
                stack_size--;
            }
            else if (stack_size == 0)
            {
                stack[stack_size++] = "..";
            }
        }
        else
        {
            stack[stack_size++] = token;
        }
        token = strtok(NULL, "/");
    }

    canonicalize_curpath(stack, stack_size, &path, curpath);

    free(copy);
    return path;
}

static int cd_hyphen(void)
{
    char *curpath = getenv("OLDPWD");
    char *pwd = calloc(PATH_MAX, sizeof(char));
    pwd = getcwd(pwd, PATH_MAX);
    if (!curpath)
    {
        update_pwd(pwd, pwd);
    }
    if (!chdir(curpath))
    {
        update_pwd(curpath, pwd);
        puts(curpath);
        free(pwd);
        return 0;
    }
    free(pwd);
    return 1;
}

int bi_cd(BI_PROTOTYPE)
{
    char *arg = *(args + 1);
    char *path = NULL;

    if (!step_1(&arg, c))
        return 0;

    char *curpath = calloc(PATH_MAX, sizeof(char));
    char *pwd = calloc(PATH_MAX, sizeof(char));
    step_2(&arg, c);

    if (*arg == '-')
    {
        if (strlen(arg) != 1)
        {
            perror("Wrong cd");
            goto Lerror;
        }
        free(pwd);
        free(curpath);
        return cd_hyphen();
    }

    if (step_3(&arg, c, &curpath))
        goto Lstep_7;

    step_6(&arg, &curpath);

Lstep_7:
    step_7(&curpath, args, &arg);

    // Step 8
    path = canonicalize_path(curpath);
    if (!path)
        goto Lerror;

    pwd = getcwd(pwd, PATH_MAX);

    if (!chdir(path))
    {
        update_pwd(path, pwd);
        CLEAN(arg, curpath, path, pwd, 0);
    }

Lerror:
    CLEAN(arg, curpath, path, pwd, 1);
}

#endif