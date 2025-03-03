#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

/** Terminates the program
 * @note This function is marked as extern "C" to disable CPP name mangling, thus making the function callable from
 * assembly
 * */
extern "C" [[noreturn]] void exit();

/** Executes a GRUB module
 *
 * @param path program path
 */
void exec(const char* path, int argc, const char** argv);

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

bool mkdir(const char* path);

bool touch(const char* path);

bool ls(const char* path);

void puts(const char* str);

void putchar(char c);

void clear_screen();

void dns(const char* domain);

#endif //INCLUDE_SYSCALLS_H
