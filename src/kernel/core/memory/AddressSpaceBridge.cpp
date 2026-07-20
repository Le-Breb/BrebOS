#include "AddressSpaceBridge.h"

namespace Memory
{
    AddressSpaceBridge::AddressSpaceBridge(Process* current_process, const Process* target_process)
    : current_process(current_process), target_process(target_process)
    {
    }

    AddressSpaceBridge::~AddressSpaceBridge()
    {
        auto r = mappings.getRoot();

        while (r)
        {
            for (uint i = 0; i < r->data.num_pages; i++)
                current_process->remove_page_mapping(r->data.start_current_pte + i);
            mappings.deleteByVal(r->data);
            r = mappings.getRoot();
        }
    }

    uintptr_t AddressSpaceBridge::convert(uintptr_t target_address, uint size)
    {
        const uint target_pte_id = ADDR_PAGE(target_address);
        const uint target_start_pte = PTE(target_process->page_tables, target_pte_id);
        if (!(target_start_pte & PAGE_PRESENT)) // Cannot point to a page that is not present
            return CONVERSION_ERROR;

        const uint offset = ADDR_PAGE_OFF(target_address);
        const uint num_pages = ADDR_PAGE(size + offset + PAGE_SIZE - 1);
        const page_mapping mapping{target_pte_id, CONVERSION_ERROR, CONVERSION_ERROR};

        uint current_pte_id;

        if (const auto node = mappings.search(mapping); node) // Mapping already exist
        {
            if (node->data.num_pages >= num_pages)
                current_pte_id = node->data.start_current_pte;
            else
                return CONVERSION_ERROR; // Mapping exists but does not cover the whole memory region
        }
        else // No mapping exists, let's create one
        {
            if ((current_pte_id = current_process->new_proc_mapping(target_pte_id, num_pages, target_process, true)) == -1U)
                return CONVERSION_ERROR;

            // Register mapping
            mappings.insert({target_pte_id, current_pte_id, num_pages});
        }

        // Return mapped address
        return PAGE_ADDR(current_pte_id) + offset;
    }
} // Memory