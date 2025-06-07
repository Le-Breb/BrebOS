#include "dentry.h"
#include <kstring.h>

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
	auto* abs_name = new char[absolute_path_length(true) + 1];
	abs_name[0] = '\0';
	[[maybe_unused]] auto _ = write_name(abs_name, true);

	return abs_name;
}

size_t Dentry::absolute_path_length(bool is_last) const
{
	// +1 for the '/' separator
	return strlen(name) + (parent ? parent->absolute_path_length(is_last) + !is_last : 0);
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
