#include "inode.h"

#include "../core/fb.h"

Inode::Inode(const Superblock* superblock, uint size, uint lba, Type type, ino_t id, nlink_t nlink, uid_t uid,
             gid_t gid, dev_t rdev, blkcnt_t blocks, time_t atime, time_t mtime, time_t ctime) : superblock(superblock),
    size(size), lba(lba), type(type), id(id), mode((type == Dir ? S_IFDIR : S_IFREG) | 0777), nlink(nlink), uid(uid),
    gid(gid), rdev(rdev), blocks(blocks), atime(atime), mtime(mtime), ctime(ctime)
{
    if (type != Dir && type != File)
        irrecoverable_error("Inode unsupported file type: %d", type);
}
