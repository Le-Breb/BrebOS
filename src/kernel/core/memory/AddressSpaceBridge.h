#pragma once

#include "../../processes/process.h"

namespace Memory
{
    class AddressSpaceBridge
    {
    protected:
        struct page_mapping
        {
            uint start_target_pte, start_current_pte;
            uint num_pages;
        };
        static constexpr int cmp_mapping(const page_mapping& a, const page_mapping& b) {return a.start_target_pte < b.start_target_pte ? -1 : (a.start_target_pte == b.start_target_pte ? 0 : 1);}

        Process* current_process;
        const Process* target_process;
        RBTree<page_mapping> mappings{cmp_mapping};
    public:
        static constexpr uintptr_t CONVERSION_ERROR = -1U;

        AddressSpaceBridge(Process* current_process, const Process* target_process);
        /**
         * Destroys the AddressSpaceBridge, removing all mappings used.
         */
        ~AddressSpaceBridge();
        uintptr_t convert(uintptr_t target_address, uint size);
    };
} // Memory
