#pragma once

#include "SlabAllocator.h"
#include "RawMemory.h"

extern void* operator new(size_t, void* p); // Idk why this is not found by default...
__attribute__ ((format (printf, 1, 2))) int printf_warn(const char* format, ...);

namespace Memory
{
    template <typename T>
    typename SlabAllocator<T>::page_header* SlabAllocator<T>::get_new_page()
    {
        const uint pe = get_free_pe();
        const uintptr_t addr = pe << 12;
        allocate_page(pe, DEFAULT_K_POLICY);

        page_header* page_header = new (reinterpret_cast<void*>(addr)) struct page_header();
        struct alloc* alloc = new (reinterpret_cast<void*>(page_header + 1)) struct alloc();

        for (uint i = 0; i < max_allocs_per_page - 1; i++)
            alloc[i].next = &alloc[i + 1];
        alloc[max_allocs_per_page - 1].next = nullptr;
        page_header->freep = &alloc[0];
        page_header->num_free_allocs = max_allocs_per_page;

        page_header->next_page_hdr = ghdr->empty_pages;
        ghdr->empty_pages = page_header;
        ++ghdr->num_empty_pages;

        return ghdr->empty_pages;
    }

    template <typename T>
    SlabAllocator<T>::SlabAllocator()
    {
        SlabAllocatorSizeChecker<SlabAllocator>{};

        ghdr = new (reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this) + sizeof(*this))) global_header();
    }

    template <typename T>
    SlabAllocator<T>::~SlabAllocator()
    {
        const uintptr_t addr = reinterpret_cast<uintptr_t>(this);
        free_page(addr);
    }

    template <typename T>
    SlabAllocator<T>* SlabAllocator<T>::get_instance()
    {
        static SlabAllocator* allocator = nullptr;

        if (allocator)
            return allocator;

        const uint pe = get_free_pe();
        allocate_page(pe, DEFAULT_K_POLICY);
        allocator = reinterpret_cast<SlabAllocator*>(pe << 12);
        allocator = new(allocator) SlabAllocator();
        return allocator;
    }

    template <typename T>
    T* SlabAllocator<T>::alloc()
    {
        page_header* page_header = ghdr->partial_pages ? ghdr->partial_pages :
            ghdr->empty_pages ? ghdr->empty_pages : get_new_page();
        if (page_header == ghdr->empty_pages)
        {
            ghdr->empty_pages = ghdr->empty_pages->next_page_hdr;
            page_header->next_page_hdr = ghdr->partial_pages;
            ghdr->partial_pages = page_header;
            --ghdr->num_empty_pages;
        }

        T* data = &page_header->freep->data;
        page_header->freep = page_header->freep->next;
        --page_header->num_free_allocs;
        if (!page_header->num_free_allocs)
        {
            ghdr->partial_pages = ghdr->partial_pages->next_page_hdr;
            page_header->next_page_hdr = ghdr->full_pages;
            ghdr->full_pages = page_header;
        }

        return data;
    }

    template <typename T>
    void SlabAllocator<T>::free(T* t)
    {
        uintptr_t page_base_addr = reinterpret_cast<uintptr_t>(t) & ~(PAGE_SIZE - 1);
        page_header* page_header = reinterpret_cast<struct page_header*>(page_base_addr);
        struct alloc* alloc = reinterpret_cast<struct alloc*>(reinterpret_cast<uintptr_t>(t) + sizeof(T) - sizeof(struct alloc));
        alloc->next = page_header->freep;

        page_header->freep = alloc;
        ++page_header->num_free_allocs;
        if (page_header->num_free_allocs == max_allocs_per_page)
        {
            ghdr->partial_pages = ghdr->partial_pages->next_page_hdr;
            page_header->next_page_hdr = ghdr->empty_pages;
            ghdr->empty_pages = page_header;
            ++ghdr->num_empty_pages;

            if (ghdr->num_empty_pages > N_FREE_PAGE_THRESHOLD)
            {
                struct page_header* to_be_freed = ghdr->empty_pages;
                ghdr->empty_pages = ghdr->empty_pages->next_page_hdr;
                free_page(ADDR_PAGE(reinterpret_cast<uintptr_t>(to_be_freed)));
            }
        }
    }
}
