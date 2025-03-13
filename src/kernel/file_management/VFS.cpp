#include "VFS.h"
#include "FAT.h"
#include "superblock.h"
#include <kstring.h>
#include "../core/fb.h"

uint VFS::lowest_free_inode = 0;
uint VFS::lowest_free_dentry = 0;
Inode* VFS::inodes[MAX_INODES] = {nullptr};
Dentry* VFS::dentries[MAX_DENTRIES] = {nullptr};
File* VFS::fds[MAX_OPEN_FILES] = {nullptr};
uint VFS::num_path = 0;
Dentry* VFS::path[PATH_CAPACITY] = {};


void VFS::init()
{
	FS::init();
	FAT_drive::init();

	mount_rootfs(*FS::fs_list->get(0));

	// Create /mnt
	Inode* mnt_node = new Inode(nullptr, 0, 0, Inode::Dir);
	Dentry* mnt_dentry = new Dentry(mnt_node, dentries[0], "mnt");
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

Dentry* VFS::touch(const char* pathname)
{
	const char* file_name;
	Dentry* parent_dentry = get_file_parent_dentry(pathname, file_name);
	if (!parent_dentry)
		return nullptr;

	auto dentry = parent_dentry->inode->superblock->get_fs()->touch(*parent_dentry, file_name);
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

	Dentry* dentry = browse_to(pathname);

	// Couldn't browse up to parent directory, abort
	if (!dentry || dentry->inode->type != Inode::Dir)
	{
		printf_error("%s no such/not a directory", pathname);
		return false;
	}

	return dentry->inode->superblock->get_fs()->ls(*dentry, ls_printer);
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
	Dentry* dentry = browse_to(parent_dir_path);
	if (!dentry)
		return false;
	// Couldn't browse up to parent directory, abort
	if (dentry->inode->type != Inode::Dir)
	{
		printf_error("%s no such/not a directory", pathname);
		return false;
	}
	delete[] parent_dir_path;

	dentry = dentry->inode->superblock->get_fs()->mkdir(*dentry, dir_name);
	if (dentry)
		cache_dentry(dentry);
	return dentry;
}

bool VFS::cat(const char* pathname)
{
	Dentry* dentry = get_file_dentry(pathname);
	if (!dentry)
		return false;

	return dentry->inode->superblock->get_fs()->cat(*dentry, file_printer);
}

bool VFS::write_buf_to_file(const char* pathname, const void* buf, uint length)
{
	Dentry* dentry = get_file_dentry(pathname, false);
	if (dentry)
	{
		printf_error("%s: already exists", pathname);
		return false;
	}

	if (!((dentry = touch(pathname))))
		return false;

	return dentry->inode->superblock->get_fs()->write_buf_to_file(*dentry, buf, length);
}

void VFS::file_printer(const void* buf, size_t len, const char* extension)
{
	if (!strcmp(extension, "txt"))
	{
		auto cbuf = (char*)buf;
		for (uint i = 0; i < len; i++)
			FB::putchar(cbuf[i]);
	}
	else
	{
		auto bbuf = (uint8_t*)buf;
		for (uint i = 0; i < len; i++)
			printf("%02x", bbuf[i]);
	}
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


Dentry* VFS::get_cached_dentry(Dentry* parent, const char* name) // Todo: use a hash map
{
	for (auto & dentry : dentries)
	{
		if (!dentry)
			continue;
		if (!strcmp(dentry->name, name) && dentry->parent == parent)
			return dentry;
	}

	return nullptr;
}

bool VFS::add_to_path(const char* path)
{
	if (lowest_free_inode >= PATH_CAPACITY)
		return false;

	Dentry* dentry = browse_to(path);
	if (!dentry || dentry->inode->type != Inode::Dir)
	{
		delete dentry;
		return false;
	}
	VFS::path[num_path++] = dentry;

	return true;
}

Dentry* VFS::browse_to(const char* path, Dentry* starting_point, bool print_errors)
{
	char* svptr; // Internal pointer for strok_r calls

	char* p = new char[strlen(path) + 1];
	strcpy(p, path);
	char* token = strtok_r(p, "/", &svptr);
	Dentry* dentry = starting_point;

	// Browse cached dentries as much as possible
	while (token)
	{
		Dentry* next_entry = get_cached_dentry(dentry, token);
		if (!next_entry) //  Nothing ffound in cache
			break;

		dentry = next_entry;
		token = strtok_r(nullptr, "/", &svptr);
		if (next_entry->inode->type != Inode::Dir) // Stop browsing if we hit a file's cached dentry
			break;
	}

	// Pure virtual node, cannot do anything there
	if (!dentry->inode->superblock)
	{
		if (print_errors)
			printf_error("Path targets full virtual Inode");
		delete[] p;
		return nullptr;
	}

	// We browsed up to a file's cached dentry but we haven't finished browsing (i.e., part of the path targets a file)
	if (token && dentry->inode->type != Inode::Dir)
	{
		if (print_errors)
			printf_error("%s no such directory", path);
		delete[] p;
		return nullptr;
	}

	// Full path cannot be fully browsed only using cached entries, now manually browse
	FS* fs = dentry->inode->superblock->get_fs();
	while (token)
	{
		if (dentry->inode->type != Inode::Dir)
		{
			if (print_errors)
				printf_error("%s not a directory", path);
			delete[] p;
			return nullptr;
		}
		dentry = fs->get_child_entry(*dentry, token);
		if (!dentry)
		{
			if (print_errors)
				printf_error("%s no such directory", path);
			delete[] p;
			return nullptr;
		}

		if (!cache_dentry(dentry))
		{
			if (print_errors)
				printf_error("Too many inodes or dentries");
			delete[] p;
			return nullptr;
		}

		token = strtok_r(nullptr, "/", &svptr);
	}
	delete[] p;

	return dentry;
}

bool VFS::cache_dentry(Dentry* dentry)
{
	if (lowest_free_dentry == MAX_DENTRIES)
		free_unused_cache_entries();
	// No need to check for lowest_free_inode cause there is one or more dentry per inode,
	// ie there cannot have more inodes than dentires

	if (lowest_free_dentry == MAX_DENTRIES || lowest_free_inode == MAX_INODES)
		return false;

	inodes[lowest_free_inode++] = dentry->inode;
	dentries[lowest_free_dentry++] = dentry;

	while (lowest_free_inode < MAX_INODES && inodes[lowest_free_inode])
		lowest_free_inode++;
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
			if (i < lowest_free_dentry)
				lowest_free_dentry = i;
			continue;
		}
		if (!dentries[i]->rc)
		{
			delete dentries[i];
			dentries[i] = nullptr;

			if (i < lowest_free_inode)
				lowest_free_dentry = i;
		}
	}
}

void VFS::free_unused_inode_cache_entries()
{
	for (uint i = 2; i < MAX_INODES; ++i) // skip root and mnt entries
	{
		if (!inodes[i])
		{
			if (i < lowest_free_inode)
				lowest_free_inode = i;
			continue;
		}
		if (!inodes[i]->rc)
		{
			delete inodes[i];
			inodes[i] = nullptr;

			if (i < lowest_free_inode)
				lowest_free_inode = i;
		}
	}
}

void VFS::free_unused_cache_entries()
{
	free_unused_dentry_cache_entries();
	free_unused_inode_cache_entries();
}

Dentry* VFS::get_file_parent_dentry(const char* pathname, const char*& file_name)
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

	Dentry* dentry = browse_to(p);
	if (!dentry || dentry->inode->type != Inode::Dir)
	{
		printf_error("%s no such/not a directory", pathname);
		return nullptr;
	}
	delete[] p;

	return dentry;
}

Dentry* VFS::get_file_dentry(const char* pathname, bool print_errors)
{
	const char* file_name;
	Dentry* parent_dentry = get_file_parent_dentry(pathname, file_name);
	if (!parent_dentry)
		return nullptr;

	return browse_to(file_name, parent_dentry, print_errors);
}

Dentry* VFS::browse_to(const char* path)
{
	if (path[0] == '\0')
	{
		printf_error("Attempting to browse to an empty path");
		return nullptr;
	}
	if (path[0] == '/')
		return browse_to(path, dentries[0]);

	for (uint i = 0; i < num_path; ++i)
	{
		Dentry* d = browse_to(path, VFS::path[i]);
		if (d)
			return d;
	}

	return nullptr;
}

void* VFS::load_file(const char* path, uint offset, uint length)
{
	if (!path)
		return nullptr;
	Dentry* dentry = browse_to(path);
	if (!dentry || dentry->inode->type != Inode::File)
	{
		printf_error("%s: no such file", path);
		return nullptr;
	}

	return dentry->inode->superblock->get_fs()->load_file_to_buf(dentry->name, dentry->parent, offset,
	                                                             length ? length : dentry->inode->size);
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
	Inode* n = fs->get_root_node();
	Dentry* d = new Dentry(n, dentries[1], mount_point + 4);

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
	if (inodes[0])
		return false;

	// Compute mount point
	char mount_point[] = {'/', '\0'};

	// Create FS superblock
	if (!Superblock::add(mount_point, fs))
		return false;

	// Get and register FS root
	Inode* n = fs->get_root_node();
	auto d = new Dentry(n, nullptr, mount_point);

	if (!cache_dentry(d))
	{
		delete d;
		delete n;

		return false;
	}

	return true;
}
