#include "PageCache.h"

#include "../fb.h"
#include "../PIT.h"
#include "../../processes/scheduler.h"
#include "../../file_management/superblock.h"

namespace Memory
{
    list<SharedPointer<Inode>> dirty_inodes;
    cached_page** frame_cache_page;

    void rmap_add(cached_page* page, pdt_t* pdt, page_table_t* pt, uint page_id)
    {
        page->rmap.add(rmap_entry{pdt, pt, page_id});
    }

    void rmap_remove(cached_page* page, page_table_t* pt, uint page_id)
    {
        if (!page->rmap.remove(rmap_entry{nullptr, pt, page_id})) [[unlikely]] // pdt is not compared
            irrecoverable_error("%s: page %u maps frame %u but is not referenced in its rmap", __func__, page_id,
                                page->frame_id);
    }

    void sync()
    {
        while (true)
        {
            PIT::sleep(1000);
            Scheduler::preemption_lock = true;

            for (auto& inode : dirty_inodes)
            {
                for (auto& page : inode->cached_pages)
                {
                    if (!page.dirty)
                        continue;

                    // Write-protect first, so that any further write faults and marks the page dirty again.
                    // No TLB invalidation needed as we run in the kernel address space
                    for (const auto& [_, pt, page_id] : page.rmap)
                        PTE(pt, page_id) &= ~PAGE_WRITE;

                    const uint file_offset = PAGE_ADDR(page.file_page_id);
                    if (file_offset < inode->size) // Otherwise page is entirely past EOF (file shrank): nothing to write
                    {
                        const size_t length = min(inode->size - file_offset, (uint)PAGE_SIZE);
                        inode->superblock->get_fs()->write_buf_to_file(inode.get(), (void*)PAGE_ADDR(page.kernel_pte), length, file_offset).expect();
                    }
                    page.dirty = false;
                }
            }
            dirty_inodes.clear();

            Scheduler::preemption_lock = false;
        }
    }
}