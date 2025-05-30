#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "sys/unistd.h"

#define BUF_LEN 1024

// Prints an error message as well as the meaning of errno and exit with an error code
void err_exit(const char* msg)
{
    perror(msg);
    _exit(1);
}

typedef void (*printer)(const void* buf, size_t len);

// interpret data as text and prints it
void text_printer(const void* buf, size_t len)
{
    auto cbuf = (char*)buf;
    write(STDOUT_FILENO, cbuf, len);
}

// interpret data as hex bytes and print it
void byte_printer(const void* buf, size_t len)
{
    auto bbuf = (uint8_t*)buf;
    for (uint i = 0; i < len; i++)
        printf("%02x", bbuf[i]);
}

// find the appropriate printer according to the file extension. Defaults to byte printer if not suitable
// printer is found
printer get_printer(const char* pathname)
{
    const char* ext = strrchr(pathname, '.');
    if (ext && strcmp(ext, ".txt") == 0)
        return text_printer;

    return byte_printer; // Use byte printer for other files
}


void cat(const char* path)
{
    int fd = open(path, O_RDONLY);

    if (fd == -1)
        err_exit("Error opening file");

    char buffer[BUF_LEN];
    auto printer = get_printer(path);

    while (auto r = read(fd, buffer, BUF_LEN))
    {
        if (r < 0)
        {
            close(fd);
            err_exit("Error reading file");
        }

        printer(buffer, r);
    }

    close(fd);
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        puts("Usage: cat <file>\n");
        return 1;
    }

    cat(argv[1]);
}