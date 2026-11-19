#include "inode.h"

#include <kstring.h>

#include "superblock.h"
#include "../core/fb.h"
#include "../core/memory/memory.h"
#include "../processes/process.h"
#include "../utils/comparison.h"

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

Status Inode::add_cached_pages(uint first_file_page, uint n_pages, bool load)
{
    if (!n_pages || n_pages > MAX_PAGES_PER_DISK_READ || (load && PAGE_ADDR(first_file_page) >= size)) [[unlikely]]
        irrecoverable_error("%s: invalid request of %u pages from page %u (size %u, load %d)", __func__, n_pages,
                            first_file_page, size, load);

    Process* kernel_process = Memory::kernel_process;
    const uint first_pte = Memory::get_contiguous_pages(n_pages, {KERNEL_VIRTUAL_BASE, false}, kernel_process->page_tables,
                                                        kernel_process->lowest_free_pe);
    if (first_pte == -1U)
        return MAKE_ERR("no kernel virtual memory left to cache %u pages", n_pages);

    // Let update_pte allocate the frames: the kernel mappings hold the cache references on them
    for (uint i = 0; i < n_pages; i++)
        kernel_process->update_pte(first_pte + i, PAGE_PRESENT | PAGE_WRITE, true);
    while (PTE_USED(kernel_process->page_tables, kernel_process->lowest_free_pe))
        kernel_process->lowest_free_pe++;

    void* mem = reinterpret_cast<void*>(PAGE_ADDR(first_pte));
    uint loaded_bytes = 0;
    if (load)
    {
        const uint file_offset = PAGE_ADDR(first_file_page);
        const uint length = min(PAGE_ADDR(n_pages), size - file_offset);
        const Status status = superblock->get_fs()->load_file_to_buf(mem, this, file_offset, length, loaded_bytes);
        if (!status.is_ok() || loaded_bytes != length)
        {
            for (uint i = 0; i < n_pages; i++)
                kernel_process->update_pte(first_pte + i, 0, true);
            kernel_process->lowest_free_pe = min(kernel_process->lowest_free_pe, first_pte);
            if (!status.is_ok())
                return status;
            return MAKE_ERR("could only load %u bytes out of %u", loaded_bytes, length);
        }
    }
    // Zero what was not loaded
    memset((char*)mem + loaded_bytes, 0, PAGE_ADDR(n_pages) - loaded_bytes);

    for (uint i = 0; i < n_pages; i++)
    {
        const uint frame = PTE_FRAME(PTE(kernel_process->page_tables, first_pte + i));
        cached_pages.addFirst({first_file_page + i, first_pte + i, frame, false, {}});
        Memory::frame_cache_page[frame] = cached_pages.get(0);
    }

    return Status::success();
}

Result<Memory::cached_page*> Inode::get_cached_page(uint file_page_id, bool load)
{
    if (Memory::cached_page* page = find_cached_page(file_page_id))
        return make_ok(page);

    TRY(add_cached_pages(file_page_id, 1, load));
    return make_ok(find_cached_page(file_page_id));
}

Status Inode::read_cached(void* buf, uint offset, uint length)
{
    if (offset > size || length > size - offset)
        return MAKE_ERR("reading %u bytes at offset %u exceeds file size %u", length, offset, size);

    uint processed_bytes = 0;
    while (processed_bytes < length)
    {
        const uint pos = offset + processed_bytes;
        const uint file_page = pos / PAGE_SIZE;
        const uint offset_in_page = ADDR_PAGE_OFF(pos);
        const uint n = min(PAGE_SIZE - offset_in_page, length - processed_bytes); // Rest of the page, within the range

        Memory::cached_page* page = find_cached_page(file_page);
        if (!page)
        {
            // Load the following uncached pages of the range along with this one, with a single disk read
            const uint last_page = (offset + length - 1) / PAGE_SIZE;
            uint n_pages = 1;
            while (n_pages < MAX_PAGES_PER_DISK_READ && file_page + n_pages <= last_page &&
                   !find_cached_page(file_page + n_pages))
                n_pages++;

            TRY(add_cached_pages(file_page, n_pages, true));
            page = find_cached_page(file_page);
        }

        memcpy((char*)buf + processed_bytes, (char*)PAGE_ADDR(page->kernel_pte) + offset_in_page, n);
        processed_bytes += n;
    }

    return Status::success();
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
