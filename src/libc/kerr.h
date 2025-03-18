#ifndef KERR_H
#define KERR_H

#define EXIT_FAILURE 1

[[noreturn]] void errx(int eval, const char *fmt, ...);
#endif //KERR_H
