#ifdef IMPLEMENTED

#include <ctype.h>
#include <sys/stat.h>

#include "headers.h"

static int get_default_fd(enum token_type redir_type)
{
    switch (redir_type)
    {
    case TOKEN_REDIR_GA:
    case TOKEN_REDIR_GO:
    case TOKEN_REDIR_GG:
    case TOKEN_REDIR_G:
        return 1;
    case TOKEN_REDIR_LG:
    case TOKEN_REDIR_LA:
    case TOKEN_REDIR_L:
        return 0;
    default:
        errx(1, "Unknown redirection type");
        break;
    }
}

/**
 * Get redirection file descriptor. Chooses between user choice or default value
 * @param redir_info redirection info
 * @return user choice or default fd value
 */
static int get_fd(struct redir_info *redir_info)
{
    return redir_info->fd == -1 ? get_default_fd(redir_info->type)
                                : redir_info->fd;
}

static int is_fd_open_for_writing(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return 0; // Fd is not open

    int access_mode = flags & O_ACCMODE;
    return access_mode == O_WRONLY || access_mode == O_RDWR;
}

static int is_fd_open_for_reading(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1)
        return 0; // Fd is not open

    int access_mode = flags & O_ACCMODE;
    return access_mode == O_RDONLY || access_mode == O_RDWR;
}

/**
 * Flush the stream represented by fd if it is stdin/out/err
 * @param fd
 */
void flush_fd(int fd)
{
    switch (fd)
    {
    case STDIN_FILENO:
        fflush(stdin);
        break;
    case STDOUT_FILENO:
        fflush(stdout);
        break;
    case STDERR_FILENO:
        fflush(stderr);
        break;
    default:
        break;
    }
}

static int dup_fd(int fd)
{
    int d = dup(fd);
    if (d == -1)
        errx(1, "dup failed");
    return d;
}
static void dup2_(int oldfd, int newfd)
{
    if (dup2(oldfd, newfd) == -1)
        errx(1, "dup2 failed");
}

// > or >> or >| // Todo: handle no clobber (cf FCL)
void exec_redir_output(EXEC_PROTOTYPE)
{
    struct redir_info *r = ast->p1;
    int fd = get_fd(r);
    if (-1 == fcntl(fd, F_GETFL))
        errx(1, "fd %u is not open\n", fd);

    // open file
    int flags =
        O_WRONLY | O_CREAT | (r->type == TOKEN_REDIR_GG ? O_APPEND : O_TRUNC);
    int file = open(r->word, flags, S_IRUSR | S_IWUSR);
    fcntl(file, F_SETFD, FD_CLOEXEC);

    // save fd
    int fd_save = dup_fd(fd);
    fcntl(fd_save, F_SETFD, FD_CLOEXEC);

    // perform redir
    dup2_(file, fd);
    close(file);

    // execute command
    exec_switch(ast->p2, c);
    flush_fd(fd);

    // restore
    dup2_(fd_save, fd);
    close(fd_save);
}

static int is_number(const char *str)
{
    while (*str && isdigit(*str))
        str++;

    return !*str;
}

// <
void exec_redir_input(EXEC_PROTOTYPE)
{
    struct redir_info *r = ast->p1;
    int fd = get_fd(r);
    if (-1 == fcntl(fd, F_GETFL))
        errx(1, "fd %u is not open\n", fd);

    // open file
    int file = open(r->word, O_RDONLY);
    if (file == -1)
        errx(1, "Failed to open %s", r->word);
    fcntl(file, F_SETFD, FD_CLOEXEC);

    // save fd
    int fd_save = dup_fd(fd);
    fcntl(fd_save, F_SETFD, FD_CLOEXEC);

    // perform redir
    dup2_(file, fd);
    close(file);

    // execute command
    exec_switch(ast->p2, c);
    flush_fd(fd);

    // restore
    dup2_(fd_save, fd);
    close(fd);
}

// >&  or <&
void exec_redir_duplicate(EXEC_PROTOTYPE)
{
    struct redir_info *r = ast->p1;
    int fd = get_fd(r);
    int out_is_number = is_number(r->word);

    if (out_is_number)
    {
        int from_fd_save = -1;
        // if from fd exists, save it
        if (fcntl(fd, F_GETFD) != -1)
        {
            from_fd_save = dup_fd(fd);
            fcntl(from_fd_save, F_SETFD, FD_CLOEXEC);
        }

        // perform redir
        int out_fd = atoi(r->word);
        if (out_fd == fd) // skip self fd copy
        {
            close(from_fd_save);
            return;
        }

        // destination fd does not exist or has wrong flags
        if (!(r->type == TOKEN_REDIR_GA ? is_fd_open_for_writing(out_fd)
                                        : is_fd_open_for_reading(out_fd)))
            errx(1,
                 "wrong redirection right hand file descriptor:"
                 "fd %d is either closed or either not opened in adequate mode",
                 out_fd);

        dup2_(out_fd, fd);
        close(out_fd);

        // execute command
        exec_switch(ast->p2, c);
        flush_fd(fd);

        // restore
        if (from_fd_save == -1)
            close(fd);
        else
        {
            dup2_(from_fd_save, fd);
            close(from_fd_save);
        }

        return;
    }

    // this is a grammar error, return code 2
    if (strcmp("-", r->word) != 0)
        errx(1, "invalid redirection target: %s\n", r->word);

    close(fd); // no error
}

// <> // Todo: find how to test this thing
void exec_int_out_redir(EXEC_PROTOTYPE)
{
    struct redir_info *r = ast->p1;
    int fd = get_fd(r);

    // open file
    int file = open(r->word, O_RDONLY);
    fcntl(file, F_SETFD, FD_CLOEXEC);

    // save fd
    int fd_save = dup_fd(fd);
    fcntl(fd_save, F_SETFD, FD_CLOEXEC);

    // apply redir
    dup2_(file, fd);
    close(file);

    // execute command
    exec_switch(ast->p2, c);
    flush_fd(fd);

    // restore
    dup2_(fd_save, fd);
    close(fd_save);
}

void exec_redir(EXEC_PROTOTYPE)
{
    if (ast->type != AST_REDIR)
        errx(1, "Not a redir");

    switch (*(enum token_type *)ast->p1)
    {
    case TOKEN_REDIR_LG:
        exec_int_out_redir(ast, c);
        break;
    case TOKEN_REDIR_LA:
    case TOKEN_REDIR_GA:
        exec_redir_duplicate(ast, c);
        break;
    case TOKEN_REDIR_GG:
    case TOKEN_REDIR_G:
    case TOKEN_REDIR_GO:
        exec_redir_output(ast, c);
        break;
    case TOKEN_REDIR_L:
        exec_redir_input(ast, c);
        break;
    default:
        errx(1, "Unexpected redir type %u\n", *(enum token_type *)ast->p1);
    }
}

#endif