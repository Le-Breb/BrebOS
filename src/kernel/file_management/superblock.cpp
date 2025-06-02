#include "superblock.h"
#include <kstring.h>

dev_t Superblock::num_devices = 0;
Superblock* Superblock::block_list[MAX_DEVICES] = {nullptr};

Superblock::Superblock(const char* mount_pathname, FS* fs) : fs(fs)
{
	this->mount_pathname = new char[strlen(mount_pathname)];
	strcpy((char*)this->mount_pathname, mount_pathname);
}

bool Superblock::add(const char* mount_pathname, FS* fs)
{
	if (num_devices == MAX_DEVICES)
		return false;

	for (dev_t i = 0; i < num_devices; ++i)
	{
		if (!strcmp(mount_pathname, block_list[i]->mount_pathname))
			return false;
	}

	Superblock* s = new Superblock(mount_pathname, fs);
	fs->superblock = s;
	block_list[num_devices++] = s;

	return true;
}

dev_t Superblock::get_num_devices()
{
	return num_devices;
}

FS* Superblock::get_fs() const
{
	return fs;
}
