#ifndef BREBOS_VFS_H
#define BREBOS_VFS_H

#include "dentry.h"

#define MAX_FS 10
#define MAX_OPEN_FILES 100
#define MAX_INODES 100
#define MAX_DENTRIES 200
#define PATH_CAPACITY 1024

#include "list.h"
#include "file.h"
#include "FS.h"

/**
 * Virtual File System. \n
 * This is an abstraction layer of physical file systems. All kernel file operations
 * are performed through this unified interface. That way, hardware specific implementation details
 * are hidden. Those details are implemented by device drivers. They have to inherit from the abstract FS
 * class, which unifies file systems representation and is at the core of the VFS routines.
 */
class VFS
{
	static uint lowest_free_inode;
	static uint lowest_free_dentry;
	static Inode* inodes[MAX_INODES]; // Caches Inodes entries. [0] = /, [1] = /mnt
	static Dentry* dentries[MAX_DENTRIES]; // Caches directory entries. [0] = /, [1] = /mnt
	static File* fds[MAX_OPEN_FILES]; // Open file descriptors
	static Dentry* path[PATH_CAPACITY];
	static uint num_path;

	/**
	 * Searches a dentry into cached ones
	 * @param parent parent of the looked after dentry
	 * @param name name of looked after dentry
	 * @return matching cached dentry, or nullptr if not found
	 */
	static Dentry* get_cached_dentry(Dentry* parent, const char* name);

	static bool add_to_path(const char* path);

	static Dentry* browse_to(const char* path, Dentry* starting_point);

	static bool cache_dentry(Dentry* dentry);

	static void free_unused_dentry_cache_entries();

	static void free_unused_inode_cache_entries();

	static void free_unused_cache_entries();

public:
	static void init();

	static bool touch(const char* pathname);

	static bool ls(const char* pathname);

	static bool mkdir(const char* pathname);

	/**
	 * Mounts a file system at /mnt/FS_index
	 * @param fs File System to mount
	 * @return boolean indicating success state
	 */
	static bool mount(FS* fs);

	/**
	 * Mounts root file system at /
	 * @param fs Root File System
	 * @return boolean indicating success state
	 */
	static bool mount_rootfs(FS* fs);

	/**
	 * Browses file system to the folder located at path
	 * @param path path to browse to
	 * @param starting_point
	 * @return Dentry of folder at path, nullptr if something went wront
	 */
	static Dentry* browse_to(const char* path);

	static void* load_file(const char* path, uint offset = 0, uint length = 0);
};


#endif //BREBOS_VFS_H
