#include "AddressSpaceBridge.h"

#include "../../utils/comparison.h"

namespace Memory
{
    AddressSpaceBridge::BridgeTree::BridgeTree(compare_func_t compare_func, allocator_t allocator,
        deallocator_t deallocator) : RBTree(compare_func, allocator, deallocator)
    {
    }

    RBTree<AddressSpaceBridge::page_mapping>::Node* AddressSpaceBridge::BridgeTree::find_region(uintptr_t start_pte) const
    {
        Node* cur = root;

        while (cur)
        {
            if (cur->data.start_target_pte <= start_pte)
            {
                if (start_pte < cur->data.start_target_pte + cur->data.num_pages)
                    return cur;

                cur = cur->right;
            }
            else
                cur = cur->left;
        }

        return cur;
    }

    AddressSpaceBridge::AddressSpaceBridge(Process* current_process, const Process* target_process)
    : current_process(current_process), target_process(target_process)
    {
    }

    AddressSpaceBridge::~AddressSpaceBridge()
    {
        while (const auto r = mappings.getRoot())
            remove_mapping(r->data);
    }

    void AddressSpaceBridge::remove_mapping(const page_mapping& mapping)
    {
        for (uint i = 0; i < mapping.num_pages; i++)
            current_process->remove_page_mapping(mapping.start_current_pte + i);
        mappings.deleteByVal(mapping);
    }

    uintptr_t AddressSpaceBridge::convert(uintptr_t target_address, uint size)
    {
        const uint target_pte_id = ADDR_PAGE(target_address);
        const uint target_start_pte = PTE(target_process->page_tables, target_pte_id);
        if (!(target_start_pte & PAGE_PRESENT)) [[unlikely]] // Cannot point to a page that is not present
            return CONVERSION_ERROR;

        const uint offset = ADDR_PAGE_OFF(target_address);
        const uint num_pages = ADDR_PAGE(size + offset + PAGE_SIZE - 1);

        // Fast path: region is already entirely mapped. Target page may be anywhere inside the mapping
        if (const auto node = mappings.find_region(target_pte_id); node)
        {
            const uint delta = target_pte_id - node->data.start_target_pte;
            if (delta + num_pages <= node->data.num_pages)
                return PAGE_ADDR(node->data.start_current_pte + delta) + offset;
        }

        // Mappings cannot overlap, so merge every mapping overlapping the region into a single one covering their union.
        // Previously returned addresses are invalidated, which is fine as callers convert again on each access
        uint start = target_pte_id;
        uint end = target_pte_id + num_pages;
        for (uint p = start; p < end; p++) // end may grow while iterating, so that mappings past the region are merged
        {
            if (const auto node = mappings.find_region(p); node)
            {
                const page_mapping mapping = node->data;
                start = min(start, mapping.start_target_pte);
                end = max(end, mapping.start_target_pte + mapping.num_pages);
                remove_mapping(mapping);
            }
        }

        const uint current_pte_id = current_process->new_proc_mapping(start, end - start, target_process, true);
        if (current_pte_id == -1U) [[unlikely]]
            return CONVERSION_ERROR;
        mappings.insert({start, current_pte_id, end - start});

        // Return mapped address
        return PAGE_ADDR(current_pte_id + (target_pte_id - start)) + offset;
    }
} // Memory