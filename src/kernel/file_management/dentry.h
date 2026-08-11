#ifndef BREBOS_DENTRY_H
#define BREBOS_DENTRY_H

#include "inode.h"
#include "../utils/optional.h"
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

	void mount_at(const SharedPointer<Dentry>& mount_point);

	// If this entry is a mount point, return the mounted directory. Otherwise returns self.
	static SharedPointer<Dentry> follow_mount(const SharedPointer<Dentry>& dentry);
private:
	[[nodiscard]]
	size_t absolute_path_length() const;

	[[nodiscard]]
	char* write_name(char* str, bool is_last) const;
public:
	SharedPointer<Inode> inode;
	SharedPointer<Dentry> parent;
	const char* name;
	Optional<SharedPointer<Dentry>> mount_pointer = nullopt;
};


#endif //BREBOS_DENTRY_H
