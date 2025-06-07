#include "VFS.h"
#include "FAT.h"
#include "superblock.h"
#include <kstring.h>
#include "../core/fb.h"
#include "../utils/comparison.h"
#include "sys/errno.h"

uint VFS::lowest_free_dentry = 0;
SharedPointer<Dentry>* VFS::dentries[MAX_DENTRIES] = {nullptr};
uint VFS::num_path = 0;
SharedPointer<Dentry>* VFS::path[PATH_CAPACITY] = {};
VFS::file_descriptor VFS::file_descriptors[MAX_FD] = {};
size_t VFS::lowest_free_fd = 0;

void VFS::init()
{
	FS::init();
	FAT_drive::init();

	for (auto& fd : file_descriptors)
		fd.fd = -1;

	mount_rootfs(*FS::fs_list->get(0));

	// Create /mnt
	Inode* mnt_node = new Inode(nullptr, 0, 0, Inode::Dir, 1, 1, 0, 0, 0, 1, 0, 0, 0);
	auto mnt_dentry = SharedPointer<Dentry>(new Dentry{mnt_node, *dentries[0], "mnt"});
	if (!cache_dentry(mnt_dentry))
	{
		printf_error("Couldn't mount mnt");
		return;
	}

	// FS limit check
	if (FS::fs_list->size()/* + num_inodes*/ >= MAX_FS)
	{
		printf_error("Too many file systems (wtf that\'s a lot)");
		return;
	}

	// Mount other File Systems
	for (int i = 1; i < FS::fs_list->size(); i++)
		mount(*FS::fs_list->get(i));

	if (!add_to_path("/bin"))
		printf_error("Failed to add /bin to path");
}

SharedPointer<Dentry> VFS::touch(const char* pathname)
{
	const char* file_name;
	SharedPointer<Dentry> parent_dentry = get_file_parent_dentry(pathname, file_name);
	if (!parent_dentry)
		return nullptr;

	auto dentry = parent_dentry->inode->superblock->get_fs()->touch(parent_dentry, file_name);
	if (dentry)
		cache_dentry(dentry);

	return dentry;
}

bool VFS::ls(const char* pathname)
{
	// Basic path checks
	if (!pathname || pathname[0] != '/')
	{
		printf_error("Invalid path");
		return false;
	}

	SharedPointer<Dentry> dentry = browse_to(pathname);

	// Couldn't browse up to parent directory, abort
	if (!dentry || dentry->inode->type != Inode::Dir)
	{
		printf_error("%s no such/not a directory", pathname);
		return false;
	}

	return dentry->inode->superblock->get_fs()->ls(dentry, ls_printer);
}

bool VFS::mkdir(const char* pathname)
{
	// Basic path checks
	if (!pathname || pathname[0] != '/' || !strcmp(pathname, "/"))
	{
		printf_error("Invalid path");
		return false;
	}

	// Compute parent folder path
	size_t path_length = strlen(pathname);
	char* parent_dir_path = new char[path_length + 1];
	strcpy(parent_dir_path, pathname);
	// Ensure path ends with a '/'
	if (pathname[path_length - 1] != '/')
	{
		parent_dir_path[path_length++] = '/';
		parent_dir_path[path_length] = '\0';
	}
	// Erase folder name to only keep the path of its parent
	uint i = path_length - 2;
	while (pathname[i] != '/')
		i--;
	memset(parent_dir_path + i + 1, 0, path_length - i);
	// Get dir name
	const char* dir_name = pathname + i + 1;

	// ~cd parent
	SharedPointer<Dentry> dentry = browse_to(parent_dir_path);
	if (!dentry)
		return false;
	// Couldn't browse up to parent directory, abort
	if (dentry->inode->type != Inode::Dir)
	{
		printf_error("%s no such/not a directory", pathname);
		return false;
	}
	delete[] parent_dir_path;

	dentry = dentry->inode->superblock->get_fs()->mkdir(dentry, dir_name);
	if (dentry)
		cache_dentry(dentry);
	return dentry;
}

char* VFS::get_absolute_path(const char* path)
{
	if (!path)
		return nullptr;
	SharedPointer<Dentry> dentry = browse_to(path);
	if (!dentry || dentry->inode->type != Inode::File)
	{
		printf_error("%s: no such file", path);
		return nullptr;
	}
	return dentry->get_absolute_path();
}

bool VFS::write_buf_to_file(const char* pathname, const void* buf, uint length)
{
	SharedPointer<Dentry> dentry = get_file_dentry(pathname, false);
	if (!dentry)
		if (!((dentry = touch(pathname))))
			return false;

	return dentry->inode->superblock->get_fs()->write_buf_to_file(dentry, buf, length);
}

void VFS::ls_printer(const Dentry& dentry)
{
	bool is_dir = dentry.inode->type == Inode::Dir;
	if (is_dir)
		FB::set_fg(FB_BROWN);
	else
		FB::set_fg(FB_MAGENTA);
	FB::write(dentry.name);
	if (is_dir)
		FB::putchar('/');
	FB::putchar('\n');
	FB::set_fg(FB_WHITE);
}

SharedPointer<Dentry> VFS::get_cached_dentry(const SharedPointer<Dentry>& parent, const char* name) // Todo: use a hash map
{
	for (auto& ddentry : dentries)
	{
		if (!ddentry)
			continue;
		auto dentry = *ddentry;
		
		if (!strcmp(dentry->name, name) && dentry->parent == parent)
			return dentry;
	}

	return nullptr;
}

bool VFS::add_to_path(const char* path)
{
	SharedPointer<Dentry> dentry = browse_to(path);
	if (!dentry || dentry->inode->type != Inode::Dir)
		return false;

	VFS::path[num_path++] = new SharedPointer<Dentry>(dentry);

	return true;
}

SharedPointer<Dentry> VFS::browse_to(const char* path, const SharedPointer<Dentry>& starting_point, bool print_errors)
{
#define exit_free() delete[] p;
#define err(...) {\
	if (print_errors) \
		printf_error(__VA_ARGS__); \
	exit_free() \
	return nullptr; \
}
	char* svptr; // Internal pointer for strok_r calls

	char* p = new char[strlen(path) + 1];
	strcpy(p, path);
	char* token = strtok_r(p, "/", &svptr);
	SharedPointer<Dentry> dentry = starting_point;

	// Browse cached dentries as much as possible
	while (token)
	{
		SharedPointer<Dentry> next_entry = strcmp(".", token) ?
			strcmp("..", token) ?
				get_cached_dentry(dentry, token) : dentry->parent
			: dentry;
		if (!next_entry) //  Nothing found in cache
			break;

		dentry = next_entry;
		token = strtok_r(nullptr, "/", &svptr);
		if (next_entry->inode->type != Inode::Dir) // Stop browsing if we hit a file's cached dentry
			break;
	}

	// Pure virtual node, cannot do anything there
	if (!dentry->inode->superblock)
		err("%s targets full virtual Inode", path);

	// We browsed up to a file's cached dentry but we haven't finished browsing (i.e., part of the path targets a file)
	if (token && dentry->inode->type != Inode::Dir)
		err("%s no such directory", path);

	// Full path cannot be fully browsed only using cached entries, now manually browse
	FS* fs = dentry->inode->superblock->get_fs();
	while (token)
	{
		if (dentry->inode->type != Inode::Dir)
			err("%s not a directory", path);
		dentry = strcmp(".", token) ?
			strcmp("..", token) ? fs->get_child_dentry(dentry, token) : dentry->parent
			: dentry;
		if (!dentry)
			err("%s no such directory", path);

		if (!cache_dentry(dentry))
		{
			if (print_errors)
				irrecoverable_error("Too many dentries");
			exit_free()
			return nullptr;
		}

		token = strtok_r(nullptr, "/", &svptr);
	}
	delete[] p;

	return dentry;
}

bool VFS::cache_dentry(const SharedPointer<Dentry>& dentry)
{
	if (lowest_free_dentry == MAX_DENTRIES)
		free_unused_dentry_cache_entries();
	// No need to check for lowest_free_inode cause there is one or more dentry per inode,
	// ie there cannot have more inodes than dentires

	if (lowest_free_dentry == MAX_DENTRIES)
		return false;

	dentries[lowest_free_dentry++] = new SharedPointer<Dentry>(dentry);

	while (lowest_free_dentry < MAX_DENTRIES && dentries[lowest_free_dentry])
		lowest_free_dentry++;

	return true;
}

void VFS::free_unused_dentry_cache_entries()
{
	for (uint i = 2; i < MAX_DENTRIES; ++i) // skip root and mnt entries
	{
		if (!dentries[i])
		{
			lowest_free_dentry = min(i, lowest_free_dentry);
			continue;
		}
		if (dentries[i]->use_count() == 1)
		{
			delete dentries[i];
			dentries[i] = nullptr;

			lowest_free_dentry = min(lowest_free_dentry, i);
		}
	}
}

SharedPointer<Dentry> VFS::get_file_parent_dentry(const char* pathname, const char*& file_name)
{
	// Basic path checks
	if (!pathname || pathname[0] != '/' || !strcmp(pathname, "/"))
	{
		printf_error("Invalid path");
		return nullptr;
	}

	// Extract parent directory path
	size_t path_length = strlen(pathname);
	char* p = new char[path_length + 1]; // Path of parent directory
	strcpy(p, pathname);
	uint i = path_length - 1;
	while (pathname[i] != '/')
		i--;
	memset(p + i + 1, 0, path_length - i);
	// Extract file name
	file_name = pathname + i + 1;
	if (!strlen(file_name))
	{
		printf_error("Empty file name");
		delete[] p;
		return nullptr;
	}

	SharedPointer<Dentry> dentry = browse_to(p);
	if (!dentry || dentry->inode->type != Inode::Dir)
	{
		printf_error("%s no such/not a directory", pathname);
		return nullptr;
	}
	delete[] p;

	return dentry;
}

SharedPointer<Dentry> VFS::get_file_dentry(const char* pathname, bool print_errors)
{
	const char* file_name;
	SharedPointer<Dentry> parent_dentry = get_file_parent_dentry(pathname, file_name);
	if (!parent_dentry)
		return nullptr;

	return browse_to(file_name, parent_dentry, print_errors);
}

SharedPointer<Dentry> VFS::browse_to(const char* path)
{
	if (path[0] == '\0')
	{
		printf_error("Attempting to browse to an empty path");
		return nullptr;
	}
	if (path[0] == '/')
		return browse_to(path, *dentries[0]);

	for (uint i = 0; i < num_path; ++i)
	{
		SharedPointer<Dentry> d = browse_to(path, *VFS::path[i]);
		if (d)
			return d;
	}

	return nullptr;
}

void* VFS::load_file(const char* path, uint offset, uint length)
{
	if (!path)
		return nullptr;
	SharedPointer<Dentry> dentry = browse_to(path);
	if (!dentry || dentry->inode->type != Inode::File)
	{
		printf_error("%s: no such file", path);
		return nullptr;
	}

	return load_file(dentry, offset, length);
}

void* VFS::load_file(const SharedPointer<Dentry>& file, uint offset, uint length)
{
	if (!file)
		return nullptr;

	uint loaded_bytes;
	uint l = length ? min(length, file->inode->size) : file->inode->size;
	void* buf =  file->inode->superblock->get_fs()->load_file_to_buf(file->name, file->parent, offset, l,
																	loaded_bytes);

	if (loaded_bytes != l)
	{
		printf_error("Could only read %u out of %u bytes", l, loaded_bytes);
		delete[] (char*)buf;
		return nullptr;
	}

	return buf;
}

int VFS::read(int fd, uint length, void* buf)
{
	if (file_descriptors[fd].fd == -1)
		return -EBADF; // fd not open

	auto& file_descriptor = file_descriptors[fd];
	auto file = file_descriptor.dentry;
	auto l = min(length, file->inode->size - file_descriptor.offset);
	uint loaded_bytes;

	if (file->inode->type != Inode::File)
		return -EINVAL; // Not a regular file

	if (!file->inode->superblock->get_fs()->load_file_to_buf(buf, file->name, file->parent, file_descriptor.offset, l,
																		loaded_bytes))
		return -EIO; // IO error

	file_descriptor.offset += loaded_bytes;


	return (int)loaded_bytes;
}

int VFS::close(int fd)
{
	if (file_descriptors[fd].fd == -1)
		return -EBADF; // File descriptor not found

	// Todo: have a proper system to manage created dentries
	// delete file_descriptors[fd].dentry; // Free the dentry associated with this fd
	file_descriptors[fd].fd = -1; // Mark as closed

	return 0; // Success
}

VFS::file_descriptor* VFS::open(const char* pathname, int flags, int local_fd, int& err)
{
	SharedPointer<Dentry> dentry = browse_to(pathname);
	if (!dentry)
	{
		err = -ENOENT; // File not found
		return nullptr;
	};

	if (dentry->inode->type == Inode::Dir)
	{
		err = -EISDIR; // Is a directory
		return nullptr;
	}

	int system_fd = get_free_fd();
	if (system_fd == -1)
	{
		err = -ENFILE; // No free file descriptors
		return nullptr;
	}

	file_descriptors[system_fd] = {
		.fd = local_fd,
		.system_fd = system_fd,
		.flags = flags,
		.offset = 0,
		.dentry = dentry
	};

	return file_descriptors + system_fd;
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

int VFS::lseek(int fd, int offset, int whence)
{
	// Check if fd is valid
	if (file_descriptors[fd].fd == -1)
		return -EBADF; // File descriptor not found

	// Check if whence is valid
	if (whence < 0 || whence > 2)
		return -EINVAL; // Invalid whence value

	// Get the file descriptor
	auto& file_descriptor = file_descriptors[fd];

	// Compute the new offset based on whence
	int ref = whence == SEEK_SET ? 0 :
		whence == SEEK_CUR ? file_descriptor.offset :
		file_descriptor.dentry->inode->size;
	int new_offset = ref + offset;

	// Check if the new offset is valid
	if (new_offset < 0)
		return -EINVAL; // Invalid offset
	if ((uint)new_offset > file_descriptor.dentry->inode->size)
	{
		printf_error("lseek called with an offset which would result in a new offset greater than file size. "
			   "This is compliant with the man page, but BrebOS VFS does not support this.");
		return -EINVAL; // Invalid offset
	}

	// Apply the new offset
	file_descriptor.offset = new_offset;

	return new_offset;
}

bool VFS::resize(SharedPointer<Dentry>& dentry, size_t new_size)
{
	return dentry->inode->superblock->get_fs()->resize(dentry, new_size);
}

int VFS::get_free_fd()
{
	for (size_t i = lowest_free_fd; i < MAX_FD; ++i)
	{
		if (file_descriptors[i].fd == -1)
		{
			lowest_free_fd = i + 1;
			while (lowest_free_fd < MAX_FD && file_descriptors[lowest_free_fd].fd != -1)
				lowest_free_fd++;
			return (int)i;
		}
	}

	return -1; // No free file descriptor
}

bool VFS::mount(FS* fs)
{
	// Compute mount point
	char mount_point[] = {'m', 'n', 't', '/', 'x', '\0'};
	mount_point[4] = '0' + Superblock::get_num_devices();

	// Create FS superblock
	if (!Superblock::add(mount_point, fs))
		return false;

	// Get and register FS root
	auto n = fs->get_root_node();
	Dentry* d = new Dentry(n, *dentries[1], mount_point + 4);

	if (!cache_dentry(d))
	{
		delete d;
		delete n;

		return false;
	}

	return true;
}

bool VFS::mount_rootfs(FS* fs)
{
	if (dentries[0])
		return false;

	// Compute mount point
	char mount_point[] = {'/', '\0'};

	// Create FS superblock
	if (!Superblock::add(mount_point, fs))
		return false;

	// Get and register FS root
	Inode* n = fs->get_root_node();
	SharedPointer<Dentry> null_parent = {nullptr};
	auto d = new Dentry(n, null_parent, mount_point);

	if (!cache_dentry(d))
	{
		delete d;
		delete n;

		return false;
	}

	return true;
}
