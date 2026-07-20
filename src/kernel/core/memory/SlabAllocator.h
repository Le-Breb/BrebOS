#pragma once

#include "MemoryDefines.h"

namespace Memory
{
    template<typename Derived>
    struct SlabAllocatorSizeChecker
    {
        /* Memory layout is
         * ================
         * SlabAllocator
         * global_header
         * =================
         * We then must ensure that the allocator and its global header fit in a single page
         */
        static_assert(sizeof(Derived) + sizeof(typename Derived::global_header) <= PAGE_SIZE);
        /* Memory layout is
         * ================
         * page_header
         * value_type_0
         * value_type_1
         * ...
         * value_type_n
         * We then must ensure that at least one value_type fits in a page after the page_header
        */
        static_assert(sizeof(typename Derived::page_header) + sizeof(struct Derived::alloc) <= PAGE_SIZE);
    };

    template<typename T>
    class SlabAllocator
    {
        friend class SlabAllocatorSizeChecker<SlabAllocator<T>>;
        struct alloc
        {
            T data; // Data MUST be at the beginning of the struct so that 'free' computations are correct
            alloc* next;
        };
        struct page_header
        {
            page_header* next_page_hdr = nullptr;
            alloc* freep = nullptr;
            uint num_free_allocs;
        };
        struct global_header
        {
            page_header* empty_pages = nullptr;
            page_header* partial_pages = nullptr;
            page_header* full_pages = nullptr;
            uint num_empty_pages = 0;
        };
        static constexpr uint max_allocs_per_page = (PAGE_SIZE - sizeof(page_header)) / sizeof(alloc);
        global_header* ghdr = nullptr;
        page_header* get_new_page();
        const uint N_FREE_PAGE_THRESHOLD = 2;
        SlabAllocator();
        ~SlabAllocator();
    public:
        static SlabAllocator* get_instance();
        T* alloc();
        void free(T* t);
    };
}



#include "SlabAllocator.hxx"
