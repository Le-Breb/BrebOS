#ifndef BREBOS_SUPERBLOCK_H
#define BREBOS_SUPERBLOCK_H

#include <kstddef.h>
#include "FS.h"
#include <sys/stat.h>

#define MAX_DEVICES 4

class Superblock
{
	static dev_t num_devices;
	static Superblock* block_list[MAX_DEVICES];

	Superblock(const char* mount_pathname, FS* fs);

	FS* fs;
public:
	[[nodiscard]] FS* get_fs() const;

	const char* mount_pathname;

	static bool add(const char* mount_pathname, FS* fs);

	[[nodiscard]]
	static dev_t get_num_devices();
};


#endif //BREBOS_SUPERBLOCK_H
