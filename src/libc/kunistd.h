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
 *
 * @return the pid of the created program, -1 on failure
 */
pid_t exec(const char* path, int argc, const char** argv);
#endif //KUNISTD_H
