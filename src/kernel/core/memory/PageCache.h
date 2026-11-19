#pragma once
#include "RawMemory.h"
#include "../../utils/list.h"

class Inode;

namespace Memory
{
    struct rmap_entry
    {
        pdt_t* pdt; // Page directory of the address space, needed to update the PTE
        page_table_t* pt;
        uint page_id;
        bool operator==(const rmap_entry& other) const {return pt == other.pt && page_id == other.page_id;};
    };

    struct cached_page
    {
        uint file_page_id;
        uint kernel_pte;
        uint frame_id;
        bool dirty;

        // Reverse map: tells every (page_table, PTE) that maps that frame
        list<rmap_entry> rmap;
    };

    // Inodes with dirty cached pages. Being referenced here keeps them alive until they are written back
    extern list<SharedPointer<Inode>> dirty_inodes;

    /**
     * Syncs dirty memory mapped inodes on disk. Never returns: this is the entry point of a kernel process
     */
    void sync();
}