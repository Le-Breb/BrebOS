#include <stdio.h>
#include <string.h>
#include <unistd.h>

[[noreturn]]
void usage_err()
{
    fprintf(stderr, "Usage: gdb <mode> <elf_name>\n");
    fprintf(stderr, "    mode is either l for load, or u for unload\n");

    _exit(2);
}



int main([[maybe_unused]] int argc, char** argv)
{
    if (argc != 3)
        usage_err();

    const bool mode_is_load = !(strcmp("l", argv[1]));
    const bool mode_is_unload = !(strcmp("u", argv[1]));

    if (!mode_is_load && !mode_is_unload)
        usage_err();


    __asm__ volatile("int $0x80" : : "a"(3), "d"(mode_is_load), "S"(argv[2]));

    return 0;
}
