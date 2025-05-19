#ifndef INCLUDE_SYSCALLS_H
#define INCLUDE_SYSCALLS_H

/**
 * Blocks the process until a key is pressed and return it
 * @return pressed key
 */
char get_keystroke();

/**
 * Gets the size of a file
 * @param path path of the file
 * @return size of the file, -1 on error
 */
unsigned int get_file_size(const char* path);

bool load_file(const char* path, void* buf);

void lock_framebuffer_flush();

void unlock_framebuffer_flush();

void get_screen_dimensions(unsigned int* width, unsigned int* height);

// Dummy function to force the linker to link libk. It is referenced in start_program.s
extern "C" void libk_force_link();

#endif //INCLUDE_SYSCALLS_H
