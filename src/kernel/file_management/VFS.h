#ifndef BREBOS_VFS_H
#define BREBOS_VFS_H

#include "dentry.h"

#define MAX_FS 10
#define MAX_OPEN_FILES 100
#define MAX_INODES 100
#define MAX_DENTRIES 200
#define PATH_CAPACITY 1024
#define MAX_FD 100
#define MAX_FD_PER_PROCESS 20

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
public:
	struct file_descriptor
	{
		int fd; // Local id
		int system_fd; // System wise id
		int flags;
		uint offset;
		Dentry* dentry;
	};
private:
	static uint lowest_free_inode;
	static uint lowest_free_dentry;
	static Inode* inodes[MAX_INODES]; // Caches Inodes entries. [0] = /, [1] = /mnt
	static Dentry* dentries[MAX_DENTRIES]; // Caches directory entries. [0] = /, [1] = /mnt
	static File* fds[MAX_OPEN_FILES]; // Open file descriptors
	static Dentry* path[PATH_CAPACITY];
	static uint num_path;
	static file_descriptor file_descriptors[MAX_FD];
	static size_t lowest_free_fd;

	/**
	 * Searches a dentry into cached ones
	 * @param parent parent of the looked after dentry
	 * @param name name of looked after dentry
	 * @return matching cached dentry, or nullptr if not found
	 */
	static Dentry* get_cached_dentry(Dentry* parent, const char* name);

	static bool add_to_path(const char* path);

	static Dentry* browse_to(const char* path, Dentry* starting_point, bool print_errors = true);

	static bool cache_dentry(Dentry* dentry);

	static void free_unused_dentry_cache_entries();

	static void free_unused_inode_cache_entries();

	static void free_unused_cache_entries();

	static Dentry* get_file_parent_dentry(const char* pathname, const char*& file_name);

	static Dentry* get_file_dentry(const char* pathname, bool print_errors = true);

	static void ls_printer(const Dentry& dentry);

	[[nodiscard]]
	static int get_free_fd();
public:
	static void init();

	static Dentry* touch(const char* pathname);

	static bool ls(const char* pathname);

	static bool mkdir(const char* pathname);

	static char* get_absolute_path(const char* path);

	/**
	 * Writes a buffer to a file. If the file already exists, it is resized to length. If it doesn't exist, it is created.
	 * @param pathname path of the file
	 * @param buf data buffer
	 * @param length length of buffer
	 * @return boolean indicating success state
	 */
	static bool write_buf_to_file(const char* pathname, const void* buf, uint length);

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
	 * @return Dentry of folder at path, nullptr if something went wront
	 */
	static Dentry* browse_to(const char* path);

	static void* load_file(const char* path, uint offset = 0, uint length = 0);

	static void* load_file(const Dentry* path, uint offset = 0, uint length = 0);

	/**
	 * Reads data from a file descriptor
	 * @param fd file descriptor to read from
	 * @param length how many bytes to read
	 * @param buf buffer to read data into
	 * @return number of bytes read on success, -2 if fd not open, -3 if not a regular file, -4 if IO error
	 */
	static int read(int fd, uint length, void* buf);

	/**
	 * Closes a file descriptor
	 * @param fd file descriptor to close
	 * @return 0 on success, -2 if fd not open
	 */
	static int close(int fd);

	/**
	 * Opens a file
	 * @param pathname path of the file to open
	 * @param flags opening parameters
	 * @param local_fd file descriptor to use in the process
	 * @param err error code to return in case of failure
	 * @return the file descriptor on success, nullptr if an error occurred
	 */
	static file_descriptor* open(const char* pathname, int flags, int local_fd, int& err);

	/**
	 * Resizes a file
	 * @param dentry dentry of the file to resize
	 * @param new_size new size of the file
	 * @return boolean indicating success of the operation
	 */
	static bool resize(Dentry& dentry, size_t new_size);
};


#endif //BREBOS_VFS_H
