#ifndef BREBOS_DENTRY_H
#define BREBOS_DENTRY_H

#include "inode.h"

class Dentry
{
public:
	Dentry(const Inode* inode, const Dentry* parent, const char* name);

	~Dentry();

public:
	const Inode* inode;
	const Dentry* parent;
	const char* name;
};


#endif //BREBOS_DENTRY_H
