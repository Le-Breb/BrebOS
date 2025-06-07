#ifndef BREBOS_FS_H
#define BREBOS_FS_H

#include "inode.h"
#include "../utils/list.h"
#include "dentry.h"
#include <sys/stat.h>

class Superblock;

#define DEV_ATA_PRIMARY_MASTER_MAJOR 3

class FS
{
	friend Superblock;

	const blksize_t block_size;

	dev_t dev;

protected:
	Superblock* superblock;

	FS(blksize_t block_size, dev_t dev);

public:
	typedef void (*ls_printer)(const Dentry& dentry);

	virtual ~FS();

	static void init();

	static list<FS*>* fs_list;

	virtual SharedPointer<Dentry> get_child_dentry(SharedPointer<Dentry>& parent_dentry, const char* entry_name) = 0;

	virtual SharedPointer<Dentry> touch(SharedPointer<Dentry>& parent_dentry, const char* entry_name) = 0;

	virtual SharedPointer<Dentry> mkdir(SharedPointer<Dentry>& parent_dentry, const char* entry_name) = 0;

	virtual bool ls(const SharedPointer<Dentry>& dentry, ls_printer printer) = 0;

	void* load_file_to_buf(const char* file_name, SharedPointer<Dentry>& parent_dentry, uint offset, uint length, uint& loaded_bytes);

	virtual bool load_file_to_buf(void* buf, const char* file_name, SharedPointer<Dentry>& parent_dentry, uint offset, uint length, uint& loaded_bytes) = 0;

	virtual bool write_buf_to_file(SharedPointer<Dentry>& dentry, const void* buf, uint length) = 0;

	virtual bool resize(SharedPointer<Dentry>& dentry, uint new_size) = 0;

	[[nodiscard]]
	virtual Inode* get_root_node() = 0;

	[[nodiscard]]
	blksize_t get_block_size() const;

	[[nodiscard]]
	dev_t get_dev() const;
};


#endif //BREBOS_FS_H
