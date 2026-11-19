#ifndef BREBOS_INODE_H
#define BREBOS_INODE_H

#include <kstddef.h>
#include <sys/stat.h>

#include "../core/memory/PageCache.h"
#include "../utils/list.h"
#include "../utils/Result.h"

class Superblock;

class Inode
{
public:
    enum Type
    {
        File,
        Dir
    };

    Inode(const Superblock* superblock, uint size, Type type, ino_t id, nlink_t nlink, uid_t uid, gid_t gid,
          dev_t rdev, blkcnt_t blocks, time_t atime, time_t mtime, time_t ctime);

    /**
     * Releases cached pages. Dirty pages cannot be written back from here: by the time this destructor runs, the
     * derived part of the inode (which the FS needs to write) is already destroyed. This is fine as long as dirty
     * inodes are referenced by the sync list until written back, which prevents their destruction.
     */
    virtual ~Inode();

    list<Memory::cached_page> cached_pages;
    
    Memory::cached_page* find_cached_page(uint file_page_id) const;

    // Reads file content through the page cache, loading missing pages in it first.
    Status read_cached(void* buf, uint offset, uint length);

    /**
     * Gets a cached page, adding it to the cache if needed
     * @param file_page_id page index in the file
     * @param load when the page is not cached: whether its content is loaded from disk (the page must start before
     * the end of file) or zeroed, for a page that is about to be entirely overwritten or lies past the end of file
     * @return the cached page, or failure if loading it failed
     */
    Result<Memory::cached_page*> get_cached_page(uint file_page_id, bool load);

    /**
     * Updates the page cache after the file shrank, so that data past the new end of file can never reappear if the
     * file grows again:
     * - the page containing the new end of file is zeroed past it
     * - pages entirely past it are unmapped from processes still mapping them
     * @param new_size new size of the file, smaller than the previous one
     */
    void truncate_cached_pages(uint new_size);

    const Superblock* superblock;
    uint size;

    const Type type;
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

private:
    // Maximum number of pages loaded by a single disk read, which bounds the size of device transfers
    static constexpr uint MAX_PAGES_PER_DISK_READ = 16;

    /**
     * Adds pages to the cache. Their frames are mapped contiguously in the kernel address space, so that they are
     * loaded with a single disk read
     * @param first_file_page index in the file of the first page, none of the pages must be cached already
     * @param n_pages number of pages, at most MAX_PAGES_PER_DISK_READ
     * @param load whether the pages are loaded from disk (the first page must start before the end of file), or zeroed.
     * Loaded pages are zeroed past the end of file
     * @return failure if no kernel virtual memory is left or the disk read failed. Nothing is added in that case
     */
    Status add_cached_pages(uint first_file_page, uint n_pages, bool load);
};


#endif //BREBOS_INODE_H
