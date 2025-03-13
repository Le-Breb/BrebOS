#ifndef BREBOS_FS_H
#define BREBOS_FS_H

#include "inode.h"
#include "../utils/list.h"
#include "dentry.h"

class Superblock;

class FS
{
	friend Superblock;

protected:

	Superblock* superblock;

public:
	typedef void (*file_printer)(const void* buf, size_t len, const char* extension);

	virtual ~FS();

	static void init();

	static list<FS*>* fs_list;

	virtual Dentry* get_child_entry(Dentry& parent_dentry, const char* entry_name) = 0;

	virtual Dentry* touch(Dentry& parent_dentry, const char* entry_name) = 0;

	virtual bool mkdir(const Dentry& parent_dentry, const char* entry_name) = 0;

	virtual bool ls(const Dentry& dentry) = 0;

	virtual void* load_file_to_buf(const char* file_name, Dentry* parent_dentry, uint offset, uint length) = 0;

	virtual bool write_buf_to_file(const Dentry& dentry, const void* buf, uint length) = 0;

	virtual bool cat(const Dentry& dentry, file_printer printer) = 0;

	[[nodiscard]] virtual Inode* get_root_node() = 0;
};


#endif //BREBOS_FS_H
