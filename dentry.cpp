#include "dentry.h"
#include "clib/string.h"

Dentry::Dentry(const Inode* inode, const Dentry* parent, const char* name) : inode(inode), parent(parent),
																			 name(new char[strlen(name) + 1])
{
	strcpy((char*) this->name, name);
}

Dentry::~Dentry()
{
	delete[] name;
}
