#include "inode.h"

Inode::Inode(const Superblock* superblock, const uint size, const uint lba, Type type) : superblock(superblock),
																						 size(size),
																						 lba(lba), type(type)
{}
