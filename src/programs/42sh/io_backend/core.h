#ifndef CORE_H
#define CORE_H

#include "headers.h"

#pragma region k_adapted
// struct IO_ctx
// {
//     FILE *file;
// };
#pragma endregion

int io_backend_init(int argc, char *argv[], struct context *c);

void io_backend_exit(void);

/* returns the next char from the input. returns -1 if eof, -2 if error */
int io_backend_get_char(void);

char *io_backend_get_line(void);

#endif /* !CORE_H */
