#include "VFS.h"
#include "FAT.h"
#include "superblock.h"
#include <kstring.h>

#include "../core/fb.h"
#include "../processes/scheduler.h"
#include "../utils/comparison.h"
#include <errno.h>
#include <fcntl.h>

#include "Disk.h"
#include "USB/SCSI.h"
#include "USB/USB.h"
#include "USB/xHCI.h"
#include "ATA.h"

VFS::dentry_cache_t* VFS::dentries = nullptr;
VFS::dentry_cache_t* VFS::mount_points = nullptr;
FileInterface* VFS::file_descriptors[MAX_FD] = {};
int VFS::lowest_free_fd = 0;

void VFS::init()
{
	ATA::init();
	xHCI::get_instance()->start();
	USB::enumerate_devices();
	display_ready_usb_devices();
	FS::init();

	dentries = new dentry_cache_t();
	mount_points = new dentry_cache_t();
	FS** main_fs = FS::fs_list->get(0);
	if (main_fs == nullptr)
		irrecoverable_error("Couldn't get main file system");
	mount_rootfs(*main_fs);

	// FS limit check
	if (FS::fs_list->size()/* + num_inodes*/ >= MAX_FS)
	{
		printf_error("Too many file systems (wtf that\'s a lot)");
		return;
	}

	// Mount other File Systems
	if (FS::fs_list->size() > 1)
		for (auto it = ++FS::fs_list->begin(); it != FS::fs_list->end(); ++it)
			mount(*it);

	if (!add_to_path("/bin"))
		printf_error("Failed to add /bin to path");

	printf_info("Starting ELF preload...");
	if (File::enable_preload)
	{
		// Preload files
		for (auto& [path, data] : File::preloads_list)
		{
			printf("    Preloading %s\n", path);
			auto file = get_file_dentry(path, true, "/");
			data = load_file(file);
		}
	}
	printf_info("ELF preload finished");
}

void VFS::shutdown()
{
	delete mount_points;
	if (dentries) // May be nullptr if initialization did not run entirely
	{
		printf_info("Shutting down VFS, %zu dentries still cached", dentries->size());
		for (uint i = 0; i < 10 && dentries->size(); i++)
			free_unused_dentry_cache_entries();
		printf_info("%zu dentries still cached after cleanup", dentries->size());
	}
	delete dentries;
}

SharedPointer<Dentry> VFS::touch(const char* pathname)
{
	const char* file_name;
	SharedPointer<Dentry> parent_dentry = get_file_parent_dentry(pathname, file_name);
	if (!parent_dentry)
		return nullptr;

	auto dentry_res = parent_dentry->inode->superblock->get_fs()->touch(parent_dentry, file_name);
	if (dentry_res.warn_is_ok())
	{
		auto dentry = std::move(dentry_res).expect();
		if (dentry)
			cache_dentry(dentry);
		return dentry;
	}

	return nullptr;
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

	return dentry->inode->superblock->get_fs()->ls(dentry, ls_printer).warn_is_ok();
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

	const auto dentry_res = dentry->inode->superblock->get_fs()->mkdir(dentry, dir_name);
	if (dentry_res.warn_is_ok())
	{
		if (dentry)
			cache_dentry(dentry);
		return true;
	}
	return false;
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
	SharedPointer<Dentry> dentry = get_file_dentry(pathname, false, nullptr);
	if (!dentry)
		if (!((dentry = touch(pathname))))
			return false;

	return dentry->inode->superblock->get_fs()->write_buf_to_file(dentry, buf, length).warn_is_ok();
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

SharedPointer<Dentry> VFS::get_cached_dentry(const SharedPointer<Dentry>& parent, const char* name)
{
	if (const auto it = dentries->find(dentry_cache_key{name, parent.get()}); it != dentries->end())
		return *it;
	return nullptr;
}

bool VFS::add_to_path(const char* path)
{
	SharedPointer<Dentry> dentry = browse_to(path);
	if (!dentry || dentry->inode->type != Inode::Dir)
		return false;

	return true;
}

SharedPointer<Dentry> VFS::browse_to(const char* path, const SharedPointer<Dentry>& starting_point, bool print_errors)
{
#define error(...) {\
	if (print_errors) \
		printf_error(__VA_ARGS__); \
	return nullptr; \
}
	char* svptr; // Internal pointer for strok_r calls

	const TmpString p(strlen(path) + 1);
	strcpy(*p, path);
	char* token = strtok_r(*p, "/", &svptr);
	SharedPointer<Dentry> dentry = Dentry::follow_mount(starting_point);

	// Browse cached dentries as much as possible
	while (token)
	{
		SharedPointer<Dentry> next_entry = strcmp(".", token) ?
			strcmp("..", token) ?
				get_cached_dentry(dentry, token) : dentry->parent
			: dentry;
		if (!next_entry) //  Nothing found in cache
			break;

		dentry = Dentry::follow_mount(next_entry);
		token = strtok_r(nullptr, "/", &svptr);
		if (next_entry->inode->type != Inode::Dir) // Stop browsing if we hit a file's cached dentry
			break;
	}

	// Pure virtual node, cannot do anything there
	if (!dentry->inode->superblock)
		error("%s targets full virtual Inode", path);

	// We browsed up to a file's cached dentry, but we haven't finished browsing (i.e., part of the path targets a file)
	if (token && dentry->inode->type != Inode::Dir)
		error("%s: no such directory", path);

	// Full path cannot be fully browsed only using cached entries, now manually browse
	FS* fs = dentry->inode->superblock->get_fs();
	while (token)
	{
		if (dentry->inode->type != Inode::Dir)
			error("%s not a directory", path);
		dentry = strcmp(".", token) ?
			strcmp("..", token) ? fs->get_child_dentry(dentry, token) : dentry->parent
			: dentry;
		if (!dentry)
			error("%s: no such directory", path);
		dentry = Dentry::follow_mount(dentry);

		if (!cache_dentry(dentry))
		{
			if (print_errors)
				irrecoverable_error("Too many dentries");
			return nullptr;
		}

		token = strtok_r(nullptr, "/", &svptr);
	}

	return dentry;
}

bool VFS::cache_dentry(const SharedPointer<Dentry>& dentry)
{
	if (dentries->size() == MAX_DENTRIES)
		free_unused_dentry_cache_entries();
	// No need to check for lowest_free_inode cause there is one or more dentry per inode,
	// ie there cannot have more inodes than dentries

	if (dentries->size() == MAX_DENTRIES)
		return false;

 	dentries->emplace(dentry);

	return true;
}

void VFS::free_unused_dentry_cache_entries()
{
	for (auto it = dentries->begin(); it != dentries->end();)
	{
		if (it->use_count() == 1)
			it = dentries->erase(it);
		else
			++it;
	}
}

SharedPointer<Dentry> VFS::get_file_parent_dentry(const char* pathname, const char*& file_name, bool print_errors)
{
	// Basic path checks
	if (!pathname || pathname[0] != '/' || !strcmp(pathname, "/"))
	{
		printf_error("Invalid path");
		return nullptr;
	}

	// Extract parent directory path
	file_name = get_file_name(pathname);
	const auto dir_len = file_name - pathname;
	const TmpString p(dir_len + 1);
	(*p)[dir_len] = '\0';
	memcpy((*p), pathname, dir_len);
	if (!strlen(file_name))
	{
		printf_error("Empty file name");
		return nullptr;
	}

	SharedPointer<Dentry> dentry = browse_to(*p, print_errors);
	if (!dentry || dentry->inode->type != Inode::Dir)
	{
		if (print_errors)
			printf_error("%s no such/not a directory", pathname);
		return nullptr;
	}

	return dentry;
}

SharedPointer<Dentry> VFS::get_file_dentry(const char* pathname, bool print_errors, const char* work_dir)
{
	const bool is_path_abs = pathname[0] == '/';
	if (!pathname || (!is_path_abs && !work_dir))
		return nullptr;
	if (!strcmp("/", pathname))
		return get_root_dentry();

	const char* file_name;
	const SharedPointer<Dentry> parent_dentry = is_path_abs ?
		get_file_parent_dentry(pathname, file_name, print_errors) : browse_to(work_dir, print_errors);
	if (!is_path_abs)
		file_name = pathname;
	if (!parent_dentry)
		return nullptr;

	return browse_to(file_name, parent_dentry, print_errors);
}

SharedPointer<Dentry> VFS::browse_to(const char* path, bool print_errors)
{
	if (path[0] == '\0')
	{
		printf_error("Attempting to browse to an empty path");
		return nullptr;
	}
	if (path[0] == '/')
		return browse_to(path, get_root_dentry(), print_errors);

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
	Result<void*> res =  file->inode->superblock->get_fs()->load_file_to_buf(file->name, file->parent, offset, l,
																	loaded_bytes);
	if (!res.warn_is_ok())
		return nullptr;
	void* buf = std::move(res).expect();

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
	auto f = file_descriptors[fd];
	if (!f)
		return -EBADF; // fd not open

	int r = f->read(buf, length);

	if (r == 0 && f->should_wait_for_data_on_read())
	{
		Scheduler::do_read_wait(Scheduler::get_running_process_pid(), f->get_write_fd());
		return read(fd, length - r, (char*)buf + r);
	}

	return r;
}

int VFS::write(int fd, void* buf, uint count)
{
	auto f = file_descriptors[fd];
	if (!f)
		return -EBADF; // fd not open

	int status = f->write(buf, count);
	if (status == -EPIPE)
		Scheduler::get_running_process()->kill(SIGPIPE);

	if (status > 0 && f->get_read_fd() != -1)
		Scheduler::wake_up_read_waiting_processes(fd, f->get_read_fd());

	return status;
}

int VFS::close(int fd)
{
	auto f = file_descriptors[fd];
	if (f == nullptr)
		return -EBADF; // File descriptor not found

	if (f->rc == 0)
	{
		const int write_fd = f->get_write_fd();
		const int read_fd = f->get_read_fd();
		delete f;
		file_descriptors[fd] = nullptr;
		lowest_free_fd = min(lowest_free_fd, fd);
		if (write_fd != -1 && read_fd != -1)
			Scheduler::wake_up_read_waiting_processes(write_fd, read_fd);
	}

	return 0; // Success
}

FileInterface* VFS::open_file(const char* pathname, int flags, mode_t mode, int& err, const char* work_dir)
{
#define open_file_leave_with_error(e) { lowest_free_fd=min((int)lowest_free_fd, system_fd); err = e; return nullptr;}
	constexpr int supported_flags = O_RDONLY | O_RDWR | O_WRONLY | O_TRUNC | O_CREAT | O_SYNC | O_APPEND;

	int system_fd = get_free_fd();
	if (system_fd == -1)
		open_file_leave_with_error(-ENFILE) // No free file descriptors

	if (const int flags_check = flags & ~supported_flags)
	{
		printf_warn("Open called with the following unsupported flags: 0x%x", flags_check);
		open_file_leave_with_error(-EINVAL);
	}

	if ((flags & O_CREAT || flags & O_TMPFILE) && mode != 0777)
		printf_warn("Open called on '%s' with the following unsupported mode: 0%o. File will be created, "
			  "but with 0777 mode", pathname, mode);


	SharedPointer<Dentry> dentry = get_file_dentry(pathname, false, work_dir);
	if (!dentry)
	{
		if (flags & O_CREAT)
		{
			dentry = touch(pathname);
			if (!dentry)
				open_file_leave_with_error(-EACCES)
		}
		else
			open_file_leave_with_error(-ENOENT) // File not found
	};

	if (flags & O_TRUNC)
	{
		if (dentry->inode->type != Inode::File)
			open_file_leave_with_error(-EINVAL);
		resize(dentry, 0);
	}

	// Truncate file if asked and permitted by flags
	if (flags & O_TRUNC && (flags & O_WRONLY || flags & O_RDWR))
		if (const auto res = dentry->inode->superblock->get_fs()->write_buf_to_file(dentry, nullptr, 0); !res.warn_is_ok())
			open_file_leave_with_error(-EIO) // IO error (I did not check if this whether it's man compliant)
	file_descriptors[system_fd] = new File(system_fd, flags, 0, dentry);

	return file_descriptors[system_fd];
}

FileInterface* VFS::open_tty(TTY::Target target, int& err)
{
#define leave_with_error(e) {err = e; return nullptr;}
	int sys_fd = get_free_fd();
	if (sys_fd == -1)
		leave_with_error(-EMFILE); // No more FDs available

	return file_descriptors[sys_fd] = new TTY(sys_fd, O_RDWR, target);
}


int VFS::lseek(int fd, int offset, int whence)
{
	// Check if fd is valid
	auto f = file_descriptors[fd];
	if (f == nullptr)
		return -EBADF; // File descriptor not found

	return f->lseek(offset, whence);
}

int VFS::fstat(int fd, struct stat* statbuf)
{
	// Check if fd is valid
	auto f = file_descriptors[fd];
	if (f == nullptr)
		return -EBADF; // File descriptor not found

	return f->fstat(statbuf);
}

bool VFS::resize(SharedPointer<Dentry>& dentry, size_t new_size)
{
	return dentry->inode->superblock->get_fs()->resize(dentry, new_size).warn_is_ok();
}

int VFS::pipe(int pipefd[2])
{
	int rfd = get_free_fd();
	if (rfd == -1)
		return -ENFILE; // No more free FD
	int wfd = get_free_fd();
	if (wfd == -1)
	{
		lowest_free_fd = min(rfd, lowest_free_fd);
		return -ENFILE;
	}
	Pipe* pipes[2];
	Pipe::create_pipe(rfd, wfd, 0, pipes);
	pipefd[0] = pipes[0]->fd;
	pipefd[1] = pipes[1]->fd;
	file_descriptors[rfd] = pipes[0];
	file_descriptors[wfd] = pipes[1];

	return 0;
}

int VFS::isatty(int fd)
{
	if (fd >= MAX_FD)
		return -EBADF;
	const auto& sys_fd = file_descriptors[fd];
	if (sys_fd == nullptr)
		return -EBADF;

	if (sys_fd->type == FileInterface::TTY)
		return 0;

	return -ENOTTY;
}

int VFS::getdents(int fd, void* buffer, size_t max_size, size_t* bytes_read)
{
	const auto sys_fd = file_descriptors[fd];
	if (sys_fd == nullptr)
		return -EBADF;

	if (sys_fd->type != FileInterface::File)
		return -ENOTDIR;

	const auto dir = (File*)sys_fd;
	if (dir->dentry->inode->type != Inode::Dir)
		return -ENOTDIR;
	auto dentry = Dentry::follow_mount(dir->dentry);

	if (!dir->dentry->inode->superblock->get_fs()->getdents(dentry, buffer, max_size, bytes_read, sys_fd->offset).warn_is_ok())
		return -EIO;

	return 0;
}

int VFS::get_free_fd()
{
	for (size_t i = lowest_free_fd; i < MAX_FD; ++i)
	{
		if (file_descriptors[i] == nullptr)
		{
			lowest_free_fd = i + 1;
			while (lowest_free_fd < MAX_FD && file_descriptors[lowest_free_fd] != nullptr)
				lowest_free_fd++;
			return (int)i;
		}
	}

	return -1; // No free file descriptor
}

const char* VFS::get_file_name(const char* pathname)
{
	if (!pathname || pathname[0] != '/')
		irrecoverable_error("Get file name expects absolute path, got '%s'", pathname);

	auto len = strlen(pathname);
	unsigned long i = len - 1;
	for (; pathname[i] != '/'; i--) {};

	return pathname + i + 1;
}

template <typename Container, typename Key>
static auto find_expect(const Container& container, const Key& key, const char* format, ...)
{
	auto it = container.find(key);
	if (it == container.end())
	{
		va_list list;
		va_start(list, format);
		irrecoverable_error_aux(format, list);
	}

	return *it;
}

SharedPointer<Dentry> VFS::get_root_dentry()
{
	return find_expect(*dentries, dentry_cache_key{"/", nullptr}, "%s: couldn't find root", __func__);
}

SharedPointer<Dentry> VFS::get_mnt_dentry()
{
	return find_expect(*dentries, dentry_cache_key{"mnt", get_root_dentry().get()}, "%s: couldn't find root", __func__);
}

/**
 * Prints a byte count in the largest unit that keeps it >= 1, with one decimal place.
 * Stays in integer arithmetic as the kernel's printf has no floating point conversion.
 */
static void print_capacity(uint64_t bytes)
{
	static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
	constexpr size_t last_unit = sizeof(units) / sizeof(*units) - 1;

	size_t unit = 0;
	uint64_t scaled = bytes;
	uint64_t remainder = 0; // Whatever the last division dropped, used for the decimal place

	while (scaled >= 1024 && unit < last_unit)
	{
		remainder = scaled % 1024;
		scaled /= 1024;
		unit++;
	}

	if (unit == 0)
		printf("%llu %s", scaled, units[unit]);
	else
		printf("%llu.%llu %s", scaled, remainder * 10 / 1024, units[unit]);
}

void VFS::display_ready_usb_devices()
{
	for (const auto& msd : USB::get_mass_storage_devices())
	{
		const auto [last_lba, block_length] = SCSI::send_read_capacity_10(&msd).expect();

		// 64-bit: both operands are 32 bit, so the product overflows for any device >= 4GB.
		// last_lba is the address of the last block, hence the +1 to get a block count.
		const uint64_t device_capacity = (uint64_t)(last_lba + 1) * block_length;
		printf(" USB drive ");
		FB::set_fg(FB_LIGHTMAGENTA);
		printf("%i", msd.device->get_slot());
		FB::set_fg(FB_WHITE);
		printf(": Mass Storage ");
		print_capacity(device_capacity);
		printf(" - ");
		FB::set_fg(FB_LIGHTRED);
		printf("%s %s\n", msd.manufacturer_name, msd.product_name);
		FB::set_fg(FB_WHITE);
	}
}

bool VFS::mount(FS* fs)
{
	// Compute mount point
	char mount_point[] = {'/', 'm', 'n', 't', '/', 'x', '\0'};
	const char mount_id = '0' + Superblock::get_num_devices();
	constexpr auto id_off = 5;
	mount_point[id_off] = mount_id;

	if (!browse_to("/mnt"))
		if (!mkdir("/mnt"))
			irrecoverable_error("%s: mkdir /mnt failed", __PRETTY_FUNCTION__);
	if (!browse_to(mount_point, false))
		if (!mkdir(mount_point))
			irrecoverable_error("%s: mkdir '%s' failed", __PRETTY_FUNCTION__, mount_point);

	// Create FS superblock
	if (!Superblock::add(mount_point, fs))
		return false;

	// Get and register FS root
	const auto n = fs->get_root_node();
	SharedPointer<Dentry> d = new Dentry(n, get_mnt_dentry(), mount_point + id_off);

	// If there's a cached dentry at that path, update it
	if (const auto it = dentries->find(dentry_cache_key{mount_point + id_off, get_mnt_dentry().get()}); it != dentries->end())
		(*it)->mount_at(d);
	// Otherwise, insert new dentry in cache
	else if (!cache_dentry(d))
		return false;
	mount_points->emplace(d);

	return true;
}

bool VFS::mount_rootfs(FS* fs)
{
	if (dentries->contains(dentry_cache_key{"/", nullptr}))
		return false;

	// Compute mount point
	char mount_point[] = {'/', '\0'};

	// Create FS superblock
	if (!Superblock::add(mount_point, fs))
		return false;

	// Get and register FS root
	SharedPointer<Inode> n = fs->get_root_node();
	SharedPointer<Dentry> null_parent = {nullptr};
	SharedPointer<Dentry> d = new Dentry(n, null_parent, mount_point);

	if (!cache_dentry(d))
		return false;
	mount_points->emplace(d);

	return true;
}
