#include "memory.h"

#include "fb.h"
#include <kstring.h>
#include "../file_management/VFS.h"

namespace Memory
{
    extern "C" void boot_page_directory();

    extern "C" void boot_page_table1();

    pdt_t* pdt = (pdt_t*)boot_page_directory;
    page_table_t* asm_pt1 = (page_table_t*)boot_page_table1;
    page_table_t* page_tables;

    // Gives page id given frame id. -1 means that frame is not allocated
    uint* frame_to_page; // uint[PT_ENTRIES * PDT_ENTRIES];
    uint lowest_free_pe;
    uint lowest_free_pe_user;
    uint lowest_free_frame = 0;

    memory_header base = {.s = {&base, 0}};
    memory_header user_base = {.s = {&user_base, 0}};
    memory_header* freep; /* Lowest free block */

    [[maybe_unused]] uint loaded_grub_modules = 0;
    GRUB_module* grub_modules;

    uint free_bytes = 0;

    extern "C" void stack_top();

    uint* stack_top_ptr = (uint*)stack_top;

    /** Tries to allocate a contiguous block of memory
     * @param num_pages_requested Size of the block in bytes
     * @param user_process
     *  @return A pointer to a contiguous block of n bytes or NULL if memory is full
     */
    void* sbrk(uint num_pages_requested, Process* user_process);

    void init_frame_to_page();

    /** Tries to allocate a contiguous block of (virtual) memory
     *
     * @param n Size to allocate. Unit: sizeof(mem_header)
     * @param user_process
     * @return Pointer to allocated memory if allocation was successful, NULL otherwise
     */
    memory_header* more_kernel(uint n, Process* user_process);

    /** Reload cr3 which will acknowledge every pte change and invalidate TLB */
    extern "C" void reload_cr3_asm();

    /** Allocate 1024 pages to store the 1024 page tables required to map all the memory. \n
     * 	The page table that maps kernel pages is moved into the newly allocated array of page tables and then freed. \n
     * 	This function also set page_tables to point to the array of newly allocated page tables
     * 	@details
     *  Page tables will be written in frames 0 to 1023. \n
     *	Page tables are virtually located on pages 1024*769 to 1024*769+1023. \n
     *	@note Any call to malloc will never return 0 on a successful allocation, as virtual address 0 maps the first page
     *	table, and will consequently never be returned by malloc if allocation is successful. \n
     *	NULL will then always be considered as an invalid address.
     *	@warning This function shall only be called once
     */
    void allocate_page_tables();

    /** Mark the pages where GRUB module is loaded as allocated
     *
     * @param multibootInfo GRUB multiboot struct pointer
     */
    void load_grub_modules(struct multiboot_info* multibootInfo);

    /**
     * Free pages in large free memory blocks
     */
    void free_release_pages(Process* user_process);

    uint get_free_frame()
    {
        while (lowest_free_frame < PDT_ENTRIES * PT_ENTRIES && FRAME_USED(lowest_free_frame))
            lowest_free_frame++;

        return lowest_free_frame;
    }

    uint get_free_pe()
    {
        while (lowest_free_pe < PDT_ENTRIES * PT_ENTRIES && PTE_USED(page_tables, lowest_free_pe))
            lowest_free_pe++;

        return lowest_free_pe;
    }

    uint get_free_pe_user()
    {
        while (lowest_free_pe_user < PDT_ENTRIES * PT_ENTRIES && PTE_USED(page_tables, lowest_free_pe_user))
            lowest_free_pe_user++;

        return lowest_free_pe_user;
    }

    void init_frame_to_page()
    {
        // Allocate space for frame_to_page
        for (size_t i = 0; i < PT_ENTRIES; i++)
        {
            PTE(page_tables, 770 * PDT_ENTRIES + i) = FRAME_ID_ADDR(PDT_ENTRIES * 2 + i) | PAGE_WRITE | PAGE_PRESENT;
            INVALIDATE_PAGE(770, i);
        }
        frame_to_page = (uint*)VIRT_ADDR(770, 0, 0);
        memset(frame_to_page, 0xFF, PDT_ENTRIES * PT_ENTRIES * sizeof(uint)); // Set frame_to_page entries to -1

        // Fill frame_to_page - kernel
        for (size_t i = 0; i < PT_ENTRIES; i++)
        {
            if (!page_tables[768].entries[i])
                continue;
            frame_to_page[i] = 768 * PDT_ENTRIES + i;
        }
        // Fill frame_to_page - page tables and frame_to_page
        for (size_t i = 0; i < PT_ENTRIES * 2; i++)
            frame_to_page[PT_ENTRIES + i] = 769 * PDT_ENTRIES + i;    }

    void allocate_page_tables()
    {
        // Save for later use
        uint asm_pt1_page_table_entry = ((uint)asm_pt1 >> 12) & 0x3FF;

        /** Allocate page 1024 + 769
         * Kernel is given pages 0 to 1023, we'll allocate page tables at pages 1024 to 2047
         * As kernel will be mapped in PDT[768] to be compliant with its start address and as page tables belong to the
         * kernel, they are allocated in the next available kernel memory space, at PDT[769]
         */
        uint new_page_frame_id = PDT_ENTRIES + 769;
        uint new_page_phys_addr = FRAME_ID_ADDR(new_page_frame_id);

        // Map it in entry 1022 of asm pt (1023 is taken by VGA buffer)
        if (asm_pt1->entries[1022])
            irrecoverable_error("Not enough space to initialize memory, kernel is too large");

        asm_pt1->entries[1022] = new_page_phys_addr | PAGE_WRITE | PAGE_PRESENT;
        //MARK_FRAME_USED(new_page_frame_id); No need, will be done in allocate_page

        // Apply changes
        INVALIDATE_PAGE(768, 1022);

        // Newly allocated page will be the first of the page tables. It will map itself and the 1023 other page tables.
        // Get pointer to newly allocated page, casting to page_table*.
        page_tables = (page_table_t*)VIRT_ADDR(768, 1022, 0);

        // Allocate pages 1024 to 2047, ie allocate space of all the page tables
        for (uint i = 0; i < PT_ENTRIES; ++i)
            PTE(page_tables, i) = FRAME_ID_ADDR(PDT_ENTRIES + i) | PAGE_WRITE | PAGE_PRESENT;

        // Indicate that newly allocated page is a page table. Map it in pdt[769]
        pdt->entries[769] = asm_pt1->entries[1022];
        asm_pt1->entries[1022] = 0; // Not needed anymore as the page it maps now maps itself as it became a page table

        // Apply changes
        INVALIDATE_PAGE(768, 1022);
        INVALIDATE_PAGE(769, 1022);

        // Update pointer using pdt[769]
        page_tables = (page_table_t*)VIRT_ADDR(769, 0, 0);

        // Let user access all page tables. Access will be defined in page table entries
        for (int i = 0; i < PDT_ENTRIES; ++i)
            pdt->entries[i] |= PAGE_USER;
        /** Clear unused page tables (all but 769 and 770)
         * Kernel code is for now referenced in PDT[768] entries, not in page_tables[768], so we can also clear it
        */
        memset(&page_tables[0].entries[0], 0, 769 * PT_ENTRIES * sizeof(uint));
        memset(&page_tables[770].entries[0], 0, (PDT_ENTRIES - 770) * PT_ENTRIES * sizeof(uint));

        // Copy asm_pt1 (the page table that maps the pages used by the kernel) in appropriated page table
        memcpy(&page_tables[768].entries[0], &asm_pt1->entries[0], PT_ENTRIES * sizeof(uint));

        // Register page tables in PDT
        for (int i = 0; i < PDT_ENTRIES; ++i)
            pdt->entries[i] = PTE_PHYS_ADDR(i) | PAGE_PRESENT | PAGE_WRITE;

        // Deallocate asm_pt1's page
        page_tables[768].entries[asm_pt1_page_table_entry] = 0;

        reload_cr3_asm(); // Apply changes | Full TLB flush is needed because we modified every pdt entry

        init_frame_to_page();

        lowest_free_pe = 771 * PDT_ENTRIES; // Kernel is only allowed to allocate in higher half
        lowest_free_frame = PT_ENTRIES * 3;
        lowest_free_pe_user = 1; // 0 is reserved for page faulting
    }

    void load_grub_modules(multiboot_info* multibootInfo)
    {
        grub_modules = (GRUB_module*)malloc(multibootInfo->mods_count * sizeof(GRUB_module));

        for (uint i = 0; i < multibootInfo->mods_count; ++i)
        {
            multiboot_module_t* module = &((multiboot_module_t*)(multibootInfo->mods_addr + KERNEL_VIRTUAL_BASE))[i];

            // Set module page as present
            uint module_start_frame_id = module->mod_start / PAGE_SIZE;
            uint mod_size = module->mod_end - module->mod_start;
            uint required_pages = mod_size / PAGE_SIZE + (mod_size % PAGE_SIZE == 0 ? 0 : 1);

            uint pe_id = lowest_free_pe;

            grub_modules[i].start_addr = ((pe_id / PDT_ENTRIES) << 22) | ((pe_id % PDT_ENTRIES) << 12);
            grub_modules[i].size = mod_size;

            // Mark module's code pages as allocated
            for (uint j = 0; j < required_pages; ++j)
            {
                uint frame_id = module_start_frame_id + j;
                allocate_page(frame_id, pe_id);

                pe_id++;
            }

            lowest_free_pe = pe_id;
        }

        loaded_grub_modules = multibootInfo->mods_count;
    }

    void init(struct multiboot_info* multibootInfo)
    {
        allocate_page_tables();
        freep = &base;

        load_grub_modules(multibootInfo);
    }

    void* sbrk(uint num_pages_requested, Process* user_process)
    {
        // Memory full
        if (lowest_free_frame == PT_ENTRIES * PDT_ENTRIES)
            return nullptr;

        uint b = user_process ? user_process->lowest_free_pe : lowest_free_pe; /* block beginning page index */
        uint e; /* block end page index + 1*/
        auto pt = user_process ? user_process->page_tables : page_tables;

        // Try to find contiguous free virtual block of memory
        while (1)
        {
            // Maximum possibly free memory is too small to fulfill request
            if (b + num_pages_requested > PDT_ENTRIES * PT_ENTRIES)
                return nullptr;

            uint rem = num_pages_requested;
            uint j = b;

            // Explore contiguous free blocks while explored block size does not fulfill the request
            while (!(PTE_USED(pt, b)) && rem > 0)
            {
                j++;
                rem -= 1;
            }

            // We have explored a free block that is big enough
            if (rem == 0)
            {
                e = j;
                break;
            }

            // There is a free virtual memory block from b to j - 1 that is too small. Page entry j is present.
            // Then next possibly free page entry is the (j + 1)th
            b = j + 1;
        }

        // Allocate pages
        if (!user_process)
        {
            for (uint i = b; i < e; ++i)
                allocate_page(get_free_frame(), i);
        }
        else
        {
            for (uint i = b; i < e; ++i)
            {
                auto sys_pe = get_free_pe();
                auto frame = get_free_frame();
                allocate_page_user(frame, sys_pe);
                PTE(pt, i) = PTE(page_tables, sys_pe);
            }
        }

        auto pe = user_process ? &user_process->lowest_free_pe : &lowest_free_pe;
        *pe = e;
        while (PTE_USED(pt, *pe))
            (*pe)++;

        // Allocated memory block virtually starts at page b. Return it.
        return (void*)VIRT_ADDR(b / PDT_ENTRIES, b % PDT_ENTRIES, 0);
    }

    memory_header* more_kernel(uint n, Process* user_process)
    {
        if (n < N_ALLOC)
            n = N_ALLOC;

        uint requested_byte_size = n * sizeof(memory_header);
        auto num_pages_requested = requested_byte_size / PAGE_SIZE + (requested_byte_size % PAGE_SIZE == 0 ? 0 : 1);
        auto* h = (memory_header*)sbrk(num_pages_requested, user_process);

        if ((int*)h == nullptr)
            return nullptr;

        uint allocated_byte_size = num_pages_requested * PAGE_SIZE;
        h->s.size = allocated_byte_size / sizeof(memory_header);

        auto free_bytes_ptr = user_process ? &user_process->free_bytes : &free_bytes;
        uint free_bytes_save = *free_bytes_ptr;
        *free_bytes_ptr = -allocated_byte_size; // Prevent free from freeing pages we just allocated
        free((void*)(h + 1), user_process);
        *free_bytes_ptr = free_bytes_save + allocated_byte_size;

        return user_process ? user_process->freep : freep;
    }

    void free_release_pages(Process* user_process)
    {
        auto free_bytes_ptr = user_process ? &user_process->free_bytes : &free_bytes;
        memory_header** beg = user_process ? &user_process->freep : &freep;
        memory_header* p = *beg;
        for (memory_header* c = (*beg)->s.ptr;; p = c, c = c->s.ptr)
        {
            // printf(" - %p", c);
            uint block_byte_size = c->s.size * sizeof(memory_header);
            uint aligned_addr_base =
                ((uint)c + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); // Start addr of first page
            uint bytes_before_page = aligned_addr_base - (uint)c;
            uint aligned_free_bytes = bytes_before_page >= block_byte_size ? 0 : block_byte_size - bytes_before_page;
            aligned_free_bytes -= aligned_free_bytes & (PAGE_SIZE - 1); // -= aligned_free_bytes % PAGE_SIZE

            // Skip small block
            if (aligned_free_bytes < PAGE_SIZE)
            {
                // Arrived at the end of the list
                if (c == *beg)
                    break;
                continue;
            }

            // printf("\nFreeing from 0x%x to 0x%x\n", aligned_addr_base, aligned_addr_base + aligned_free_bytes);

            unsigned n_pages = aligned_free_bytes >> 12; // Num pages to free, aligned_free_bytes / PAGE_SIZE
            unsigned free_size = aligned_free_bytes / sizeof(memory_header);
            // Freed memory size in sizeof(memory_header)
            uint remaining_size = c->s.size - free_size;

            // Adjust memory headers
            // Page aligned block entirely removed (remaining_size can't be 0 if block isn't page aligned)
            if (remaining_size == 0)
                p->s.ptr = c->s.ptr; // Link previous to next
            else // The block is split into two blocks: one before freed pages and one after them
            {
                uint first_block_size = (aligned_addr_base - (uint)c) / sizeof(memory_header);
                uint second_block_size = remaining_size - first_block_size;

                // First block - May not exist if block is page aligned, but write anyway
                // If the block exists, info is useful, otherwise this memory location will simply be deallocated
                c->s.size = first_block_size;
                // For now c->s.ptr points to the next block in the list

                // Second block - May not exist and writing anyway would actually overwrite next block, so we ensure it exists
                if (second_block_size != 0)
                {
                    memory_header* next = c->s.ptr; // Save next
                    auto second_block = c + first_block_size + free_size;

                    // Link previous block to second block - previous block is p if first block does bot exist
                    (first_block_size == 0 ? p : c)->s.ptr = second_block;

                    // Setup second block
                    second_block->s.size = second_block_size; // Write size
                    second_block->s.ptr = next; // Link with next
                }
            }
            // Rollback c to previous mem header cause the mem region c is in may be deallocated,
            // thus the for loop would segfault without this instruction. With it, the for loop
            // will gently iterate to the next mem header as we just updated previous' next
            c = *beg = p;

            // Free pages
            for (uint i = 0; i < n_pages; ++i)
            {
                uint aligned_addr = aligned_addr_base + (i << 12);
                free_page(aligned_addr, user_process);
            }

            *free_bytes_ptr -= aligned_free_bytes; // Update free_bytes
        }

        // printf("\n====\n");
    }

    void allocate_page(uint frame_id, uint page_id)
    {
        // Write PTE
        PTE(page_tables, page_id) = FRAME_ID_ADDR(frame_id) | PAGE_WRITE | PAGE_PRESENT;
        __asm__ volatile("invlpg (%0)" : : "r" (frame_id * PAGE_SIZE));
        MARK_FRAME_USED(frame_id, page_id); // Internal allocation registration
    }

    void free_page(uint address, Process* user_process)
    {
        uint pde = address >> 22;
        uint pte = (address >> 12) & 0x3FF;
        uint sys_page_id, frame_id;
        if (user_process)
        {
            uint page_id = address / PAGE_SIZE;
            auto physical_address = PHYS_ADDR(user_process->page_tables, address);
            frame_id = physical_address / PAGE_SIZE;
            sys_page_id = frame_to_page[frame_id];
            PTE(user_process->page_tables, page_id) = 0;
            INVALIDATE_PAGE(pde, pte);
        }
        else
        {
            sys_page_id = pde * PT_ENTRIES + pte;
            frame_id = PHYS_ADDR(page_tables, address) / PAGE_SIZE;
        }

        // Write PTE
        PTE(page_tables, sys_page_id) = 0;
        if (!user_process)
            INVALIDATE_PAGE(pde, pte);
        MARK_FRAME_FREE(frame_id); // Internal deallocation registration

        if (frame_id < lowest_free_frame)
            lowest_free_frame = frame_id;
    }

    void allocate_page_user(uint frame_id, uint page_id)
    {
        allocate_page(frame_id, page_id);
        PTE(page_tables, page_id) |= PAGE_USER;
    }

    void* malloca(uint size)
    {
        uint total_size = size + sizeof(void*) + PAGE_SIZE;
        void* base_addr = malloc(total_size);
        if (base_addr == nullptr)
            return nullptr;

        uint aligned_addr = ((uint)base_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        ((void**)aligned_addr)[-1] = base_addr;

        return (void*)aligned_addr;
    }

    void freea(void* ptr)
    {
        void* base_addr = ((void**)ptr)[-1];
        free(base_addr);
    }

    uint* get_stack_top_ptr()
    {
        return stack_top_ptr;
    }

    void* mmap(int prot, const char* path, uint offset, size_t length)
    {
        if (prot)
        {
            printf_error("Prot not supported yet");
            return nullptr;
        }

        return VFS::load_file(path, offset, length);
    }

    bool identity_map(uint addr, uint size)
    {
        uint page_beg = addr & ~(PAGE_SIZE - 1);
        uint num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        for (uint i = 0; i < num_pages; ++i)
        {
            uint pde = page_beg >> 22;
            uint pte = (page_beg >> 12) & 0x3FF;
            uint frame_id = page_beg / PAGE_SIZE;
            if (page_tables[pde].entries[pte] & PAGE_PRESENT || FRAME_USED(frame_id))
                return false;
            page_beg += PAGE_SIZE;
        }

        page_beg = addr & ~(PAGE_SIZE - 1);
        uint frame_id = page_beg / PAGE_SIZE;
        for (uint i = 0; i < num_pages; ++i)
        {
            allocate_page(frame_id, frame_id);
            frame_id++;
        }

        return true;
    }

    void* physically_aligned_malloc(uint n)
    {
        uint page_beg = (uint)-1;
        uint frame_beg = (uint)-1;
        uint num_pages = (n + PAGE_SIZE - 1) / PAGE_SIZE;
        for (auto p = lowest_free_pe; p < PDT_ENTRIES * PT_ENTRIES; p++)
        {
            uint i;
            for (i = 0; i < num_pages && !PTE_USED(page_tables, p + i); i++){};
            if (i == num_pages)
            {
                page_beg = p;
                break;
            }
        }
        if (page_beg == (uint)-1)
            return nullptr;

        for (auto p = lowest_free_frame; p < PDT_ENTRIES * PT_ENTRIES; p++)
        {
            uint i;
            for (i = 0; i < num_pages && !FRAME_USED(p + i); i++){};
            if (i == num_pages)
            {
                frame_beg = p;
                break;
            }
        }
        if (frame_beg == (uint)-1)
            return nullptr;

        for (uint i = 0; i < num_pages; ++i)
            allocate_page(frame_beg + i,  page_beg + i);

        return (void*)(page_beg << 12);
    }
}

using namespace Memory;

void* operator new(size_t size)
{
    return malloc(size);
}

void* operator new[](size_t size)
{
    return malloc(size);
}

void operator delete(void* p)
{
    free(p);
}

void operator delete[](void* p)
{
    free(p);
}

void operator delete(void* p, [[maybe_unused]] long unsigned int size)
{
    free(p);
}

void operator delete[]([[maybe_unused]] void* p, [[maybe_unused]] long unsigned int size)
{
    irrecoverable_error("this operator shouldn't be used");
}

void operator delete([[maybe_unused]] void* p, [[maybe_unused]] unsigned int n)
{
    free(p);
}

void operator delete []([[maybe_unused]] void* p, [[maybe_unused]] unsigned int n)
{
    free(p);
}

void* malloc(uint n, Process* user_process)
{
    if (!n)
        return nullptr;

    memory_header* c;
    memory_header** free_list_beg = user_process ? &user_process->freep : &freep;
    memory_header* p = *free_list_beg;
    unsigned nunits = (n + sizeof(memory_header) - 1) / sizeof(memory_header) + 1;
    auto free_bytes_ptr = user_process ? &user_process->free_bytes : &free_bytes;

    for (c = p->s.ptr;; p = c, c = c->s.ptr)
    {
        if (c->s.size >= nunits)
        {
            if (c->s.size == nunits)
                p->s.ptr = c->s.ptr;
            else
            {
                c->s.size -= nunits;
                c += c->s.size;
                c->s.size = nunits;
            }
            *free_bytes_ptr -= nunits * sizeof(memory_header);
            *free_list_beg = p;
            return (void*)(c + 1);
        }
        if (c == *free_list_beg) /* wrapped around free list */
        {
            if ((c = more_kernel(nunits, user_process)) == nullptr)
                return nullptr; /* none left */
        }
    }
}

extern "C" void* malloc(uint n)
{
    return malloc(n, nullptr);
}

void* calloc(size_t nmemb, size_t size, Process* user_process)
{
    if (!nmemb || !size)
        return malloc(1, user_process);
    size_t total_size;
    if (__builtin_mul_overflow(nmemb, size, &total_size))
        return nullptr;

    void* mem = malloc(total_size, user_process);
    if (!mem)
        return nullptr;
    memset((char*)mem, 0, total_size);

    return mem;
}

extern "C" void* calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size, nullptr);
}

void free(void* ptr, Process* user_process)
{
    if (ptr == nullptr)
        return;

    memory_header* c = (memory_header*)ptr - 1;
    memory_header* p;
    auto free_bytes_ptr = user_process ? &user_process->free_bytes : &free_bytes;
    *free_bytes_ptr += c->s.size * sizeof(memory_header);
    memory_header** free_list_beg = user_process ? &user_process->freep : &freep;

    // Loop until p < c < p->s.ptr
    for (p = *free_list_beg; !(c > p && c < p->s.ptr); p = p->s.ptr)
        //Break when arrived at the end of the list and c goes before beginning or after end

        if (p >= p->s.ptr && (c < p->s.ptr || c > p))
            break;

    // Join with upper neighbor if contiguous
    if (c + c->s.size == p->s.ptr)
    {
        c->s.size += p->s.ptr->s.size;
        c->s.ptr = p->s.ptr->s.ptr;
    }
    else
        c->s.ptr = p->s.ptr;

    // Join with lower neighbor if contiguous
    if (p + p->s.size == c)
    {
        p->s.size += c->s.size;
        p->s.ptr = c->s.ptr;
    }
    else
        p->s.ptr = c;

    // Set new freep
    *free_list_beg = p;

    // Free pages
    if (*free_bytes_ptr >= FREE_THRESHOLD)
        free_release_pages(user_process);
}

extern "C" void free(void* ptr)
{
    free(ptr, nullptr);
}

void* realloc(void* ptr, size_t size, Process* user_process)
{
    if (!ptr) // realloc on null = malloc
        return malloc(size, user_process);
    if (!size) // realloc with size 0 = free
    {
        free(ptr, user_process);
        return nullptr;
    }

    // Get header
    memory_header* h = (memory_header*)ptr - 1;

    // Block is big enough
    if ((h->s.size - 1) * sizeof(memory_header) >= size) // (-1 because size accounts for the memory header itself)
        return ptr;

    memory_header* p; // previous header
    memory_header** free_list_beg = user_process ? &user_process->freep : &freep;

    // Loop until p < h < p->s.ptr
    for (p = *free_list_beg; !(h > p && h < p->s.ptr); p = p->s.ptr)
        //Break when arrived at the end of the list and c goes before beginning or after end
            if (p >= p->s.ptr && (h < p->s.ptr || h > p))
                break;

    memory_header* next = p->s.ptr;
    auto block_content_byte_size = (h->s.size - 1) * sizeof(memory_header);
    // Check if there is a contiguous free block whose combined size if large enough to contain 'size' bytes
    if  (h + h->s.size == next && block_content_byte_size + next->s.size * sizeof(memory_header) >= size)
    {
        h->s.size += next->s.size; // Add size of contiguous free block in current block

        // Update free block chain
        p->s.ptr = next->s.ptr;
        *free_list_beg = p;

        return ptr;
    }

    // We have no choice left but to copy data in a newly allocated buffer and free original data
    void *new_mem = malloc(size, user_process);

    if (!new_mem)
        return ptr; // If realloc fails, the man page indicates it should return the unchanged original buffer

    memcpy(new_mem, ptr, block_content_byte_size);
    free(ptr, user_process);

    return new_mem;

}


void* realloc(void* ptr, size_t size)
{
    return realloc(ptr, size, nullptr);
}