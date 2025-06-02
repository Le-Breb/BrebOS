#ifndef BREBOS_INODE_H
#define BREBOS_INODE_H

#include <kstddef.h>
#include <sys/stat.h>

class Superblock;

class Inode
{
public:
    enum Type
    {
        File,
        Dir
    };

    Inode(const Superblock* superblock, uint size, uint lba, Type type, ino_t id, nlink_t nlink, uid_t uid, gid_t gid,
          dev_t rdev, blkcnt_t blocks, time_t atime, time_t mtime, time_t ctime);

public:
    const Superblock* superblock;
    uint size;
    const uint lba; // Logical block address, will be interpreted as needed by FS driver

    const Type type;
    int rc = 0;
    ino_t id;
    mode_t mode;
    nlink_t nlink; // Number of hard links to this inode
    uid_t uid; // User ID of owner
    gid_t gid; // Group ID of owner
    dev_t rdev; // Device ID (if this inode is a device file, otherwise 0)
    blkcnt_t blocks; // Number of 512B blocks allocated to this inode
    time_t atime; // Last access time
    time_t mtime; // Last modification time
    time_t ctime; // Last status change time
};


#endif //BREBOS_INODE_H
