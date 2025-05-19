#include "ksyscalls.h"

char get_keystroke()
{
	int keystroke;
	__asm__ volatile ("int $0x80" : "=a"(keystroke) : "a"(4));

	return (char) keystroke;
}

unsigned int get_file_size(const char* path)
{
	unsigned int file_size;
	__asm__ volatile ("int $0x80" : "=a"(file_size) : "a"(29), "D"(path));
	return file_size;
}

bool load_file(const char* path, void* buf)
{
	bool success;
	__asm__ volatile ("int $0x80" : "=a"(success) : "a"(25), "D"(path), "S"(buf));
	return success;
}

void lock_framebuffer_flush()
{
	__asm__ volatile("int $0x80" : : "a"(22)); // Lock framebuffer flush
}

void unlock_framebuffer_flush()
{
	__asm__ volatile("int $0x80" : : "a"(23)); // Unlock framebuffer flush
}

void get_screen_dimensions(unsigned int* width, unsigned int* height)
{
	unsigned int w, h;
	__asm__ volatile("int $0x80" : "=a"(w), "=D"(h) : "a"(24));
	*width = w;
	*height = h;
}

void libk_force_link()
{
}
