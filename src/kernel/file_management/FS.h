#ifndef BREBOS_FS_H
#define BREBOS_FS_H

#include "inode.h"
#include "../utils/list.h"
#include "../utils/Result.h"
#include "dentry.h"
#include <sys/stat.h>

#include "BlockDevice.h"

class Superblock;

#define DEV_ATA_PRIMARY_MASTER_MAJOR 3
#define DEV_USB_MASS_STORAGE_MAJOR   7

class FS
{
	friend Superblock;

protected:
	Superblock* superblock;

	BlockDevice* dev;
	const blksize_t block_size;

	explicit FS(blksize_t block_size, BlockDevice* dev);

public:
	typedef void (*ls_printer)(const Dentry& dentry);

	virtual ~FS();

	static void init();

	static list<FS*>* fs_list;

	virtual SharedPointer<Dentry> get_child_dentry(SharedPointer<Dentry>& parent_dentry, const char* entry_name) = 0;

	virtual Result<SharedPointer<Dentry>> touch(SharedPointer<Dentry>& parent_dentry, const char* entry_name) = 0;

	virtual Result<SharedPointer<Dentry>> mkdir(SharedPointer<Dentry>& parent_dentry, const char* entry_name) = 0;

	virtual Status ls(const SharedPointer<Dentry>& dentry, ls_printer printer) = 0;

	virtual Status getdents(const SharedPointer<Dentry>& dentry, void* buffer, size_t max_size, size_t* bytes_read,
	                        uint& fd_off) = 0;

	Result<void*> load_file_to_buf(const char* file_name, SharedPointer<Dentry>& parent_dentry, uint offset,
	                               uint length, uint& loaded_bytes);

	virtual Status load_file_to_buf(void* buf, const char* file_name, SharedPointer<Dentry>& parent_dentry, uint offset,
	                                uint length, uint& loaded_bytes) = 0;

	virtual Status write_buf_to_file(SharedPointer<Dentry>& dentry, const void* buf, uint length) = 0;

	virtual Status resize(SharedPointer<Dentry>& dentry, uint new_size) = 0;

	[[nodiscard]]
	virtual SharedPointer<Inode> get_root_node() = 0;

	[[nodiscard]]
	blksize_t get_block_size() const;

	[[nodiscard]]
	BlockDevice* get_device() const;
};


#endif //BREBOS_FS_H
