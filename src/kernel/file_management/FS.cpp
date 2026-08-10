#include "FS.h"

FS::FS(blksize_t block_size, BlockDevice* dev) : superblock(nullptr), dev(dev), block_size(block_size)
{
}

FS::~FS() = default;

list<FS*>* FS::fs_list = nullptr;

void FS::init()
{
	fs_list = new list<FS*>();
}

Result<void*> FS::load_file_to_buf(const char* file_name, SharedPointer<Dentry>& parent_dentry, uint offset,
                                   uint length, uint& loaded_bytes)
{
	// If reading an empty file, just return a dummy vector without solicitation of the disk
	if (length == 0)
		return make_ok<void*>(new char[1]);

	auto buf = new char[length];

	const auto res = load_file_to_buf(buf, file_name, parent_dentry, offset, length, loaded_bytes);
	if (res.is_ok())
		return make_ok((void*)buf);
	delete[] buf;
	return res;
}

blksize_t FS::get_block_size() const
{
	return block_size;
}

BlockDevice* FS::get_device() const
{
	return dev;
}
