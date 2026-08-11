#include "dentry.h"
#include <kstring.h>

[[noreturn]]
__attribute__ ((format (printf, 1, 2)))
extern int irrecoverable_error(const char* format, ...);

Dentry::Dentry(const SharedPointer<Inode>& inode, const SharedPointer<Dentry>& parent, const char* name) : inode(inode),
                                                                                                           parent(parent), name(new char[strlen(name) + 1])
{
	strcpy((char*)this->name, name);
}

Dentry::~Dentry()
{
	delete[] name;
}

char* Dentry::get_absolute_path() const
{
	auto* abs_name = new char[absolute_path_length()];
	abs_name[0] = '\0';
	[[maybe_unused]] auto _ = write_name(abs_name, true);

	return abs_name;
}

TmpString Dentry::get_absolute_path_tmp() const
{
	auto abs_name = TmpString(absolute_path_length());
	(*abs_name)[0] = '\0';
	[[maybe_unused]] auto _ = write_name(*abs_name, true);

	return abs_name;
}

void Dentry::mount_at(const SharedPointer<Dentry>& mount_point)
{
	if (inode->type != Inode::Dir || mount_point->inode->type != Inode::Dir)
		irrecoverable_error("Trying to mount from/on a non-directory Inode");
	if (mount_pointer)
		irrecoverable_error("Trying to mount on a Dentry that is already a mount point");
	mount_pointer = mount_point;
}

SharedPointer<Dentry> Dentry::follow_mount(const SharedPointer<Dentry>& dentry)
{
	return dentry->mount_pointer ? *dentry->mount_pointer : dentry;
}

size_t Dentry::absolute_path_length() const
{
	// +1 for the '/' separator
	return strlen(name) + 1 + (parent ? parent->absolute_path_length() : 0);
}

char* Dentry::write_name(char* str, bool is_last) const
{
	if (parent)
		str = parent->write_name(str, false);
	const char* n = name;
	while (*n)
		*str++ = *n++;
	if (parent && !is_last)
		*str++ = '/';
	*str = '\0';
	return str;
}
