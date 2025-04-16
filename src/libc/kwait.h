#ifndef WAIT_H
#define WAIT_H
typedef int pid_t;

/**
* Wait for a certain process to terminate.
* The process' return value is written at *wstatus. If the process exited anormally, -1 is returned.
*/
pid_t waitpid(pid_t pid, int* wstatus);
#endif //WAIT_H
