#include "RawMemory.h"

#include "fb.h"
#include "abi-bits/vm-flags.h"
#include "../utils/comparison.h"

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

    void free_page(uint page_id)
    {
        const uint frame_id = PHYS_ADDR(page_tables, page_id << 12) >> 12;
        // Todo: make that cleaner, there are two free_page functions now, everybody manipulates frame_rc...
        // Write PTE in kernel global memory mapping
        auto pte_ptr = &PTE(page_tables, page_id);
        bool lazy = *pte_ptr & PAGE_LAZY_ZERO;
        if (!lazy && frame_id == 0)
            irrecoverable_error("wut");
        *pte_ptr = 0;

        if (!lazy)
            MARK_FRAME_FREE(frame_id); // Internal deallocation registration
    }

    void allocate_page(uint frame_id, uint page_id, int policy)
    {
        // Write PTE
        PTE(page_tables, page_id) = FRAME_ID_ADDR(frame_id) | policy;
        __asm__ volatile("invlpg (%0)" : : "r" (frame_id << 12));

        if (policy & PAGE_PRESENT)
            MARK_FRAME_USED(frame_id, page_id);
    }

    void allocate_page(uint page_id, int policy)
    {
        allocate_page(get_free_frame(), page_id, policy);
    }

    uint get_contiguous_pages(uint n, const hint_info& hint_info, const page_table_t* pt, uint lowest_free_page_entry)
    {
        if (hint_info.is_mandatory)
        {
            if (hint_info.hint & (PAGE_SIZE - 1))
                irrecoverable_error("%s: alloc_params->hint is not page aligned (0x%x)", __func__, hint_info.hint);
            uint b = ADDR_PAGE(hint_info.hint);
            uint pte = b;
            if (pte + n > PDT_ENTRIES * PT_ENTRIES)
                return (uint)-1;

            uint target = pte + n;
            for (; !(PTE_USED(pt, pte)) && pte != target; pte++) {}

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
