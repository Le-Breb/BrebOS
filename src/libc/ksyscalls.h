#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

typedef int pid_t;

/** Executes a program
 *
 * @param path program path
 */
void exec(const char* path, int argc, const char** argv);

/**
 * Gets PID
 * @return PID
 */
pid_t getpid();

/**
 * Blocks the process until a key is pressed and return it
 * @return pressed key
 */
char get_keystroke();

[[noreturn]] void shutdown();

bool mkdir(const char* path);

bool touch(const char* path);

bool ls(const char* path);

void puts(const char* str);

void putchar(char c);

void clear_screen();

void dns(const char* domain);

void wget(const char* uri);

void cat(const char* path);

#endif //INCLUDE_SYSCALLS_H
