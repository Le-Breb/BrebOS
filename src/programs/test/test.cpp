#include <sys/stat.h>

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    struct stat statbuf;
    stat("/img2.jpg", &statbuf);

    return 0;
}
