#include "VFS.h"
#include "FAT.h"
#include "superblock.h"
#include "clib/string.h"
#include "clib/stdio.h"

uint VFS::num_inodes = 0;
uint VFS::num_dentries = 0;
Inode* VFS::inodes[MAX_INODES] = {nullptr};
Dentry* VFS::dentries[MAX_DENTRIES] = {nullptr};
File* VFS::fds[MAX_OPEN_FILES] = {nullptr};

void VFS::init()
{
	FS::init();
	FAT_drive::init();

	mount_rootfs((FS*) FS::fs_list->get_at(0));

	// Create /mnt
	Inode* mnt_node = new Inode(nullptr, 0, 0, Inode::Dir);
	inodes[num_inodes++] = mnt_node;
	Dentry* mnt_dentry = new Dentry(mnt_node, dentries[0], "mnt");
	dentries[num_dentries++] = mnt_dentry;

	// FS limit check
	if (FS::fs_list->get_size()/* + num_inodes*/ >= MAX_FS)
	{
		printf_error("Too many file systems (wtf that\'s a lot)");
		return;
	}

	// Mount other File Systems
	for (uint i = 1; i < FS::fs_list->get_size(); ++i)
	{
		FS* fs = (FS*) FS::fs_list->get_at(i);
		mount(fs);
	}
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


Dentry* VFS::get_cached_dentry(Dentry* parent, const char* name, Inode::Type type) // Todo: use a hash map
{
	for (uint i = 0; i < num_dentries; ++i)
	{
		if (!strcmp(dentries[i]->name, name) && dentries[i]->parent == parent && dentries[i]->inode->type == type)
			return dentries[i];
	}

	return nullptr;
}

Dentry* VFS::browse_to(const char* path)
{
	char* svptr; // Internal pointer for strok_r calls

	char* p = new char[strlen(path) + 1];
	strcpy(p, path);
	char* token = strtok_r(p, "/", &svptr);
	Dentry* dentry = dentries[0];

	// Browse cached dentries as much as possible
	while (token)
	{
		Dentry* next_entry = get_cached_dentry(dentry, token, Inode::Dir);
		if (!next_entry)
			break;

		dentry = next_entry;
		token = strtok_r(nullptr, "/", &svptr);
	}

	// Pure virtual node, cannot do anything there
	if (!dentry->inode->superblock)
	{
		printf_error("Path targets full virtual Inode");
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

		if (num_inodes == MAX_INODES || num_dentries == MAX_DENTRIES)
		{
			printf_error("Too many inodes or dentries");
			delete[] p;
		}

		// Register node/dentry in cache
		inodes[num_inodes++] = dentry->inode;
		dentries[num_dentries++] = dentry;

		token = strtok_r(nullptr, "/", &svptr);
	}
	delete[] p;

	return dentry;
}

void* VFS::load_file(const char* path, uint offset, uint length)
{
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
	inodes[num_inodes++] = n;
	dentries[num_dentries++] = d;

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
	inodes[num_inodes++] = n;
	dentries[num_dentries++] = d;

	return true;
}
