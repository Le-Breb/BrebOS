#ifndef BREBOS_DENTRY_H
#define BREBOS_DENTRY_H

#include "inode.h"

class Dentry
{
public:
	Dentry(Inode* inode, Dentry* parent, const char* name);

	~Dentry();

public:
	Inode* inode;
	Dentry* parent;
	const char* name;
	int rc = 0;
};


#endif //BREBOS_DENTRY_H
