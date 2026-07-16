#ifndef BREBOS_DENTRY_H
#define BREBOS_DENTRY_H

#include "inode.h"
#include "../utils/shared_pointer.h"
#include "../utils/TmpString.h"

class Dentry
{
public:
	Dentry(const SharedPointer<Inode>& inode, const SharedPointer<Dentry>& parent, const char* name);

	~Dentry();

	[[nodiscard]]
	char* get_absolute_path() const;

	[[nodiscard]]
	TmpString get_absolute_path_tmp() const;

private:
	[[nodiscard]]
	size_t absolute_path_length() const;

	[[nodiscard]]
	char* write_name(char* str, bool is_last) const;
public:
	SharedPointer<Inode> inode;
	SharedPointer<Dentry> parent;
	const char* name;
};


#endif //BREBOS_DENTRY_H
