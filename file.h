#ifndef BREBOS_FILE_H
#define BREBOS_FILE_H


#include "clib/stddef.h"

#define F_ERR ((uint)-1)

enum Mode
{
	READ,
	WRITE,
	READWRITE,
	UNKNOWN
};

// This is more or less a copy of the DirEntry class with more methods, but
// this will be useful later to implement a proper VFS, that would be storage device independent
class File
{
	Mode mode;

	static uint num_open_files;

	static enum Mode strmode_to_mode(const char* strmode);

public:
	size_t read(void* ptr, size_t size, size_t nmemb);

	size_t write(const void* ptr, size_t size, size_t nmemb);

	bool seek(long offset, int whence);

	static bool close();

	static File* open(const char* pathname, const char* mode);
};


#endif //BREBOS_FILE_H
