#include "RawMemory.h"

#include "../fb.h"
#include "abi-bits/vm-flags.h"
#include "../../utils/comparison.h"

[[noreturn]]
extern __attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...);

namespace Memory
{
    pdt_t* pdt = (pdt_t*)boot_page_directory;
    page_table_t* asm_pt1 = (page_table_t*)boot_page_table1;
    page_table_t* page_tables;

    // Frame reference counter
    uint* frame_rc; // uint[PT_ENTRIES * PDT_ENTRIES];

    // Gives page id given frame id. -1 means that frame is not allocated
    uint* frame_to_page; // uint[PT_ENTRIES * PDT_ENTRIES];
    uint lowest_free_pe_user;
    uint* lowest_free_pe = nullptr;
    uint lowest_free_frame = 0;

    [[maybe_unused]] uint loaded_grub_modules = 0;
    GRUB_module* grub_modules;

    extern "C" void stack_top();

    uint* stack_top_ptr = (uint*)stack_top;

    const page_info DEFAULT_K_PAGE_INFO{DEFAULT_K_FLAGS, DEFAULT_K_POLICY};
    const page_info DEFAULT_U_PAGE_INFO{DEFAULT_U_FLAGS, DEFAULT_U_POLICY};
    const hint_info DEFAULT_HINT_INFO{0, false};

    uint get_free_frame()
    {
        while (lowest_free_frame < PDT_ENTRIES * PT_ENTRIES && FRAME_USED(lowest_free_frame))
            lowest_free_frame++;

        return lowest_free_frame;
    }

    uint get_free_pe()
    {
        while (*lowest_free_pe < PDT_ENTRIES * PT_ENTRIES && PTE_USED(page_tables, *lowest_free_pe))
            (*lowest_free_pe)++;

        return *lowest_free_pe;
    }

    /**
     * Writes a page table entry and releases the frame previously mapped by it.
     * The frame referenced by val, if any, MUST already account for this mapping in frame_rc: pt may be lazily
     * allocated, in which case accessing it page faults, and the fault handler would otherwise be free to pick that
     * very frame to back the page table.
     */
    static void write_pte(uint page_id, pdt_t* pdt, page_table_t* pt, uint val, bool update_cache)
    {
        // Decrease previously mapped frame reference count if there were one being referenced
        const uint previous_pte = PTE(pt, page_id);
        const uint previous_frame = PTE_FRAME(previous_pte);
        if (frame_rc[previous_frame])
        {
            frame_rc[previous_frame]--;
            if (previous_frame && !frame_rc[previous_frame])
                MARK_FRAME_FREE(previous_frame); // Internal deallocation registration
        }
        else if (previous_pte & PAGE_PRESENT && !(previous_pte & PAGE_LAZY_ZERO))
            irrecoverable_error("%s: page %u maps frame %u which is not referenced", __func__, page_id, previous_frame);

        PTE(pt, page_id) = val;

        const uint pde = page_id >> 10;
        if (!pdt->entries[pde])
        {
            pdt->entries[pde] = PHYS_ADDR(Memory::page_tables, (uint) &pt[pde]) | PAGE_USER | PAGE_WRITE |
                PAGE_PRESENT;
            if (update_cache)
                reload_cr3_asm();
        }
        else if (update_cache)
            INVALIDATE_PAGE(pde, page_id);
    }

    static bool maps_frame(uint flags)
    {
        return flags & PAGE_PRESENT && !(flags & PAGE_LAZY_ZERO);
    }

    void update_page(uint page_id, pdt_t* pdt, page_table_t* pt, uint frame_id, uint flags, bool update_cache)
    {
        if (flags & ~PTE_FLAGS_MASK) [[unlikely]]
            irrecoverable_error("%s: invalid flags 0x%x", __func__, flags);

        if (maps_frame(flags))
        {
            // Take the reference before touching pt, see write_pte
            if (!frame_rc[frame_id]) [[unlikely]]
                irrecoverable_error("%s: frame %u is not referenced", __func__, frame_id);
            frame_rc[frame_id]++;
        }
        else if (frame_id) [[unlikely]]
            irrecoverable_error("%s: frame %u provided for a mapping without frame (flags 0x%x)", __func__, frame_id,
                                flags);

        write_pte(page_id, pdt, pt, FRAME_ID_ADDR(frame_id) | flags, update_cache);
    }

    void update_page(uint page_id, pdt_t* pdt, page_table_t* pt, uint flags, bool update_cache)
    {
        if (flags & ~PTE_FLAGS_MASK) [[unlikely]]
            irrecoverable_error("%s: invalid flags 0x%x", __func__, flags);

        uint frame_id = 0;
        if (maps_frame(flags))
        {
            // Mark the frame used before touching pt, see write_pte
            frame_id = get_free_frame();
            MARK_FRAME_USED(frame_id);
        }

        write_pte(page_id, pdt, pt, FRAME_ID_ADDR(frame_id) | flags, update_cache);
    }

    void allocate_page(uint frame_id, uint page_id, int policy)
    {
        if (policy & PAGE_PRESENT)
            MARK_FRAME_USED(frame_id);

        // Write PTE
        PTE(page_tables, page_id) = FRAME_ID_ADDR(frame_id) | policy;
        __asm__ volatile("invlpg (%0)" : : "r" (PAGE_ADDR(page_id)));
    }

    void allocate_page(uint page_id, int policy)
    {
        allocate_page(policy & PAGE_PRESENT ? get_free_frame() : 0, page_id, policy);
    }

    uint get_contiguous_pages(uint n, const hint_info& hint_info, const page_table_t* pt, uint lowest_free_page_entry)
    {
        if (hint_info.is_mandatory)
        {
            if (!IS_PAGE_ALIGNED(hint_info.hint))
                irrecoverable_error("%s: alloc_params->hint is not page aligned (0x%x)", __func__, hint_info.hint);
            uint b = ADDR_PAGE(hint_info.hint);
            uint pte = b;
            if (pte + n > PDT_ENTRIES * PT_ENTRIES)
                return (uint)-1;

            uint target = pte + n;
            for (; pte != target && !(PTE_USED(pt, pte)); pte++) {}

            return pte == target ? b : (uint)-1;
        }

        uint b = max(lowest_free_page_entry, ADDR_PAGE(hint_info.hint));

        while (true)
        {
            // Maximum possibly free memory is too small to fulfill request
            if (b + n > PDT_ENTRIES * PT_ENTRIES)
                return (uint)-1;

            uint pte = b;
            uint target = b + n;

            // Explore contiguous free blocks while explored block size does not fulfill the request
            for (; !(PTE_USED(pt, pte)) && pte != target; pte++) {}

            // We have explored a free block that is big enough
            if (pte == target)
                return b;

            // There is a free virtual memory block from b to pte - 1 that is too small. Page entry pte - 1 is present.
            // Then next possibly free page entry is the (pte)th
            b = pte + 1;
        }
    }
}
