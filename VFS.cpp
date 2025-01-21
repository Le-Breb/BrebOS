#include "VFS.h"
#include "FAT.h"
#include "superblock.h"
#include "clib/string.h"
#include "clib/stdio.h"

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

	mount_rootfs((FS*)FS::fs_list->get_at(0));

	// Create /mnt
	Inode* mnt_node = new Inode(nullptr, 0, 0, Inode::Dir);
	Dentry* mnt_dentry = new Dentry(mnt_node, dentries[0], "mnt");
	if (!cache_dentry(mnt_dentry))
	{
		printf_error("Couldn't mount mnt");
		return;
	}

	// FS limit check
	if (FS::fs_list->get_size()/* + num_inodes*/ >= MAX_FS)
	{
		printf_error("Too many file systems (wtf that\'s a lot)");
		return;
	}

	// Mount other File Systems
	for (uint i = 1; i < FS::fs_list->get_size(); ++i)
	{
		FS* fs = (FS*)FS::fs_list->get_at(i);
		mount(fs);
	}

	if (!add_to_path("/bin"))
		printf_error("Failed to add /bin to path");
}

bool VFS::touch(const char* pathname)
{
	// Basic path checks
	if (!pathname || pathname[0] != '/' || !strcmp(pathname, "/"))
	{
		printf_error("Invalid path");
		return false;
	}

	// Extract parent directory path
	size_t path_length = strlen(pathname);
	char* p = new char[path_length + 1]; // Path of parent directory
	strcpy(p, pathname);
	uint i = path_length - 1;
	while (pathname[i] != '/')
		i--;
	memset(p + i, 0, path_length - i);
	// Extract file name
	const char* file_name = pathname + i + 1;
	if (!strlen(file_name))
	{
		printf_error("Empty file name");
		delete[] p;
		return false;
	}

	Dentry* dentry = browse_to(p);
	if (!dentry || dentry->inode->type != Inode::Dir)
	{
		printf_error("%s no such/not a directory", pathname);
		return false;
	}
	delete[] p;

	return dentry->inode->superblock->get_fs()->touch(*dentry, file_name);
}

bool VFS::ls(const char* pathname)
{
	// Basic path checks
	if (!pathname)
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

	return dentry->inode->superblock->get_fs()->ls(*dentry);
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
	// Couldn't browse up to parent directory, abort
	if (dentry->inode->type != Inode::Dir)
	{
		printf_error("%s no such/not a directory", pathname);
		return false;
	}
	delete[] parent_dir_path;
	if (!dentry)
		return false;

	return dentry->inode->superblock->get_fs()->mkdir(*dentry, dir_name);
}


Dentry* VFS::get_cached_dentry(Dentry* parent, const char* name) // Todo: use a hash map
{
	for (uint i = 0; i < MAX_DENTRIES; ++i)
	{
		if (!dentries[i])
			continue;
		if (!strcmp(dentries[i]->name, name) && dentries[i]->parent == parent)
			return dentries[i];
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

Dentry* VFS::browse_to(const char* path, Dentry* starting_point)
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
		printf_error("Path targets full virtual Inode");
		delete[] p;
		return nullptr;
	}

	// We browsed up to a file's cached dentry but we haven't finished browsing (i.e., part of the path targets a file)
	if (token && dentry->inode->type != Inode::Dir)
	{
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
			printf_error("%s not a directory", path);
			delete[] p;
			return nullptr;
		}
		dentry = fs->get_child_entry(*dentry, token);
		if (!dentry)
		{
			printf_error("%s no such directory", path);
			delete[] p;
			return nullptr;
		}

		if (!cache_dentry(dentry))
		{
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

Dentry* VFS::browse_to(const char* path)
{
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
	Dentry* d = new Dentry(n, nullptr, mount_point);

	if (!cache_dentry(d))
	{
		delete d;
		delete n;

		return false;
	}

	return true;
}
