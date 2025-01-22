#ifndef BREBOS_INODE_H
#define BREBOS_INODE_H

#include "libc/stddef.h"

class Superblock;

class Inode
{
public:
	enum Type
	{
		File,
		Dir
	};

	Inode(const Superblock* superblock, uint size, uint lba, Type type);

public:
	const Superblock* superblock;

	const uint size;

	const uint lba; // Logical block address, will be interpreted as needed by FS driver

	const Type type;

	int rc = 0;
};


#endif //BREBOS_INODE_H
