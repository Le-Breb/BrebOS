#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

/** Prints a string */
void print(char* str);

/** Terminates the program */
void exit();

/** Executes a GRUB module
 *
 * @param module_id Module ID
 */
void start_process(unsigned int module_id);

/**
 * Sets current process last in ready queue
 */
void pause();

/**
 * Gets PID
 * @return PID
 */
unsigned int get_pid();

#endif //INCLUDE_SYSCALLS_H
