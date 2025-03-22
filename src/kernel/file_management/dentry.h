#ifndef BREBOS_DENTRY_H
#define BREBOS_DENTRY_H

#include "inode.h"

class Dentry
{
public:
	Dentry(Inode* inode, Dentry* parent, const char* name);

	~Dentry();

	char* get_absolute_path() const;

private:
	[[nodiscard]] size_t absolute_path_length(bool is_last) const;

	[[nodiscard]] char* write_name(char* str, bool is_last) const;
public:
	Inode* inode;
	Dentry* parent;
	const char* name;
	int rc = 0;
};


#endif //BREBOS_DENTRY_H
