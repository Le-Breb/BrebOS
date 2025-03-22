#ifndef KUNISTD_H
#define KUNISTD_H

typedef int pid_t;

/**
 * Gets PID
 * @return PID
 */
pid_t getpid();

/** Executes a program
 *
 * @param path program path
 */
void exec(const char* path, int argc, const char** argv);
#endif //KUNISTD_H
