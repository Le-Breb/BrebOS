#include <kstdio.h>
#include <kstring.h>
#include <kstdlib.h>
#include <kstdint.h>

struct wget_args
{
    const char* uri;
    const char* hostname;
    uint16_t port;
};

void wget(const wget_args* args)
{
    __asm__ volatile("int $0x80" :  : "a"(15), "D"(args->uri), "S"(args->hostname), "d"(args->port));
}

void usage_err()
{
    puts("Usage: wget [OPTIONS] URI\n");
}

void show_usage()
{
    puts("Usage: wget [OPTIONS] URI (/ for root)\n");
    puts("Options:\n");
    puts("  -h <hostname>  Set the hostname to connect to (default: www.example.com)\n");
    puts("  -p <port>      Set the port to connect to (default: 80)\n");
    puts("  -H             Show this help message\n");
}

bool is_digit(const char* str)
{
    while (*str)
    {
        if (*str < '0' || *str > '9')
            return false;
        str++;
    }
    return true;
}

#define PARSING_OK 1
#define PARSING_ERR 2
#define PARSING_EXIT 3

int parse_args(int argc, char* argv[], wget_args& args)
{
    if (argc < 2)
    {
        show_usage();
        return PARSING_ERR;
    }

    args.hostname = "www.example.com";
    args.port = 80;

    if (!strcmp(argv[argc - 1], "-H"))
    {
        show_usage();
        return PARSING_EXIT;
    }

    int cargc = 1;
    while (argc - cargc >= 2)
    {
        const char* opt = argv[cargc++];
        if (strlen(opt) != 2 || opt[0] != '-')
        {
            fprintf(stderr, "Invalid option: %s\n", opt);
            return PARSING_ERR;
        }
        switch (opt[1])
        {
            case 'h':
                args.hostname = argv[cargc++];
                break;
            case 'p':
              if (!is_digit(argv[cargc]))
                    return PARSING_ERR;
                args.port = (uint16_t)atoi(argv[cargc++]);
                break;
            default:
                fprintf(stderr, "Invalid option: %s\n", opt);
                return PARSING_ERR;
        }
    }

    if (cargc != argc - 1)
    {
        if (cargc == argc)
            show_usage();
        else
          fprintf(stderr, "Invalid option: %s\n", argv[cargc]);
        return PARSING_ERR;
    }

    args.uri = argv[argc - 1];

    return PARSING_OK;
}

int main(int argc, char* argv[])
{
    wget_args args;
    auto parsing = parse_args(argc, argv, args);
    if (parsing == PARSING_ERR)
    {
        usage_err();
        return 1;
    }
    if (parsing != PARSING_EXIT)
        wget(&args);

    return 0;
}