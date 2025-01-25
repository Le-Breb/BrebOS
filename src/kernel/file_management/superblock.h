#ifndef BREBOS_SUPERBLOCK_H
#define BREBOS_SUPERBLOCK_H

#include <kstddef.h>
#include "FS.h"

#define MAX_DEVICES 4

class Superblock
{
	static uint num_devices;
	static Superblock* block_list[MAX_DEVICES];

	Superblock(const char* mount_pathname, FS* fs);

	FS* fs;
public:
	[[nodiscard]] FS* get_fs() const;

	const char* mount_pathname;

	static bool add(const char* mount_pathname, FS* fs);

	[[nodiscard]] static uint get_num_devices();
};


#endif //BREBOS_SUPERBLOCK_H
