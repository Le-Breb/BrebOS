#ifndef KUNISTD_H
#define KUNISTD_H
#include "kstddef.h"

typedef int pid_t;

/**
 * Gets PID
 * @return PID
 */
pid_t getpid();

/** Executes a program
 *
 * @param path program path
 *
 * @return the pid of the created program, -1 on failure
 */
pid_t exec(const char* path, int argc, const char** argv);

ssize_t read(int fd, void* buf, size_t count);

ssize_t write(int fd, const void* buf, size_t count);
#endif //KUNISTD_H
