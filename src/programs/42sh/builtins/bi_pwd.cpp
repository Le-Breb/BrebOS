#pragma region k_adapted
#include "headers.h"

int bi_pwd(BI_PROTOTYPE)
{
    UNUSED(args);
    UNUSED(c);

    char* wd = getcwd(NULL, 0);
    if (wd)
        printf("%s\n", wd);

    return wd != NULL;
}
#pragma endregion