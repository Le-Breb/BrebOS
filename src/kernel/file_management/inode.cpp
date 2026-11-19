#include "inode.h"

#include "../core/fb.h"
#include "../core/memory/memory.h"
#include "../processes/process.h"

Inode::Inode(const Superblock* superblock, uint size, Type type, ino_t id, nlink_t nlink, uid_t uid,
             gid_t gid, dev_t rdev, blkcnt_t blocks, time_t atime, time_t mtime, time_t ctime) : superblock(superblock),
                                                                                                 size(size), type(type), id(id), mode((type == Dir ? S_IFDIR : S_IFREG) | 0777), nlink(nlink), uid(uid),
                                                                                                 gid(gid), rdev(rdev), blocks(blocks), atime(atime), mtime(mtime), ctime(ctime)
{
    if (type != Dir && type != File)
        irrecoverable_error("Inode unsupported file type: %d", type);
}

Memory::cached_page* Inode::find_cached_page(uint file_page_id) const
{
    for (auto& page : cached_pages)
    {
        if (page.file_page_id == file_page_id)
            return &page;
    }

    return nullptr;
}

Inode::~Inode()
{
    for (const auto& [file_page_id, kernel_pte, frame_id, dirty, rmap] : cached_pages)
    {
        // No process PTE should still map a cached page by the time this destructor is called: inodes are kept alive
        // by the mappings that reference them
        if (rmap.size() > 0) [[unlikely]]
            irrecoverable_error("%s: inode %llu is being destroyed with cached page %u still mapped by %d PTEs",
                                __func__, id, file_page_id, rmap.size());

        // Dirty inodes are kept alive by the sync list until written back, and they cannot be written from here
        if (dirty) [[unlikely]]
            irrecoverable_error("%s: inode %llu is being destroyed with dirty cached page %u", __func__, id, file_page_id);

        // Unregister the frame, then drop the cache reference on it
        Memory::frame_cache_page[frame_id] = nullptr;
        Memory::kernel_process->update_pte(kernel_pte, 0, true);
    }
}
