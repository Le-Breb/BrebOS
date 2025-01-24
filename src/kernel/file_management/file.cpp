#include "file.h"
#include "FAT.h"
#include "kstring.h"

uint File::num_open_files = 0;

Mode File::strmode_to_mode(const char* strmode)
{
	if (!strcmp(strmode, "r"))
		return READ;
	if (!strcmp(strmode, "w"))
		return WRITE;
	if (!strcmp(strmode, "wr"))
		return READWRITE;

	return UNKNOWN;
}
