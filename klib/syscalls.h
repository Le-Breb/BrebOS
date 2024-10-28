#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

/**
 * Prints a formatted string
 * @param format format string
 * @param ... format arguments
 * @return Number of characters written
 */
__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...);

/** Terminates the program
 * @note This function is marked as extern "C" to disable CPP name mangling, thus making the function callable from
 * assembly
 * */
extern "C" [[noreturn]] void exit();

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

[[noreturn]] void shutdown();

/**
 * Tries to allocate n contiguous bytes of memory on the heap
 * @param n number of bytes to allocate
 * @return pointer to beginning of allocated memory block or NULL if allocation failed
 */
void* malloc(unsigned int n);

/**
 * Frees a memory block
 * @param ptr pointer to the beginning of the block
 */
void free(void* ptr);

bool mkdir(unsigned int drive_id, const char* path);

bool touch(unsigned int drive_id, const char* path);

bool ls(unsigned int drive_id, const char* path);

#endif //INCLUDE_SYSCALLS_H
