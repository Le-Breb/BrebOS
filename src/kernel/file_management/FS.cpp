#include "FS.h"

FS::~FS() = default;

list<FS*>* FS::fs_list = nullptr;

void FS::init()
{
	fs_list = new list<FS*>();
}

void* FS::load_file_to_buf(const char* file_name, Dentry* parent_dentry, uint offset, uint length, uint& loaded_bytes)
{
	auto buf = new char[length];

	if (load_file_to_buf(buf, file_name, parent_dentry, offset, length, loaded_bytes))
		return buf;

	delete[] buf;
	return nullptr;
}
