#include "env_vars.h"

char **expand_env(const char *str, struct context *c)
{
    UNUSED(c);
    char **env = (char**)calloc(2, sizeof(char *));
    if (!strcmp(str, "PWD"))
    {
        char *pwd = getenv("PWD");
        env[0] = pwd ? strdup(pwd) : strdup("");
    }
    else if (!strcmp(str, "OLDPWD"))
    {
        char *oldpwd = getenv("OLDPWD");
        env[0] = oldpwd ? strdup(oldpwd) : strdup("");
    }
    else if (!strcmp(str, "IFS"))
    {
        char *ifs = getenv("IFS");
        env[0] = ifs ? strdup(ifs) : strdup("");
    }
    if (env[0] == NULL)
    {
        free(env);
        env = NULL;
    }
    return env;
}
