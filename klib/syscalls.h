#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

/**
 * Prints a formatted string
 * @param format format string
 * @param ... format arguments
 * @return Number of characters written
 */
__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...);

/** Terminates the program */
void exit();

/** Executes a GRUB module
 *
 * @param module_id Module ID
 */
void start_process(unsigned int module_id);

/**
 * Gets PID
 * @return PID
 */
unsigned int get_pid();

/**
 * Blocks the process until a key is pressed and return it
 * @return pressed key
 */
char get_keystroke();

_Noreturn void shutdown();

#endif //INCLUDE_SYSCALLS_H
