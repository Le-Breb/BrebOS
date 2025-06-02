#include "memory.h"

#include "fb.h"
#include <kstring.h>

#include <stdint.h>
#include "../file_management/VFS.h"
#include "../processes/scheduler.h"
#include "../utils/comparison.h"

namespace Memory
{
    extern "C" void boot_page_directory();

    extern "C" void boot_page_table1();

    pdt_t* pdt = (pdt_t*)boot_page_directory;
    page_table_t* asm_pt1 = (page_table_t*)boot_page_table1;
    page_table_t* page_tables;

    // Frame reference counter
    uint* frame_rc; // uint[PT_ENTRIES * PDT_ENTRIES];

    // Gives page id given frame id. -1 means that frame is not allocated
    uint* frame_to_page; // uint[PT_ENTRIES * PDT_ENTRIES];
    uint lowest_free_pe_user;
    uint lowest_free_frame = 0;

    [[maybe_unused]] uint loaded_grub_modules = 0;
    GRUB_module* grub_modules;

    Process* kernel_process = nullptr;

    extern "C" void stack_top();

    uint* stack_top_ptr = (uint*)stack_top;

    template void Memory::allocate_page_user<false>(uint);
    template void Memory::allocate_page_user<false>(uint, uint);
    template void Memory::allocate_page<false>(uint);
    template void Memory::allocate_page<false>(uint, uint);

    /** Tries to allocate a contiguous block of memory
     * @param num_pages_requested Size of the block in bytes
     * @param process process to get the relevant address space from to correctly interpret the address, nullptr
     * if kernel
     * @param lazy_zero whether we want memory zeroed out (triggers lazy allocation)
     *  @return A pointer to a contiguous block of n bytes or NULL if memory is full
     */
    void* sbrk(uint num_pages_requested, Process* process, bool lazy_zero);

    /** Initializes frame to page mapping
     *
     * This functions allocates PDT 770 to store frame_to_page, which maps frame ids to page ids.
     * Then it fills the mapping for frames used by the kernel itself as well as and the page tables, which is the only
     * thing dynamically allocated at this point.
     */
    void init_frame_to_page();

    /** Initializes frame reference counter
     *
     * This functions allocates PDT 771 to store frame_rc, which maps frame ids to their reference count.
     * Then it fills the mapping for frames used by the kernel itself as well as and the page tables and frame_to_page,
     * which are the only things dynamically allocated at this point.
     */
    void init_frame_rc();

    /** Tries to allocate a contiguous block of (virtual) memory
     *
     * @param n Size to allocate. Unit: sizeof(mem_header)
     * @param process process to get the relevant address space from to correctly interpret the address, nullptr
     * if kernel
     * @param lazy_zero whether we want memory zeroed out (triggers lazy allocation)
     * @return Pointer to allocated memory if allocation was successful, NULL otherwise
     */
    memory_header* more_kernel(uint n, Process* process, bool lazy_zero);

    /** Allocate 1024 pages to store the 1024 page tables required to map all the memory in PDT[769]. \n
     * 	The page table that maps kernel pages is moved into the newly allocated array of page tables and then freed. \n
     * 	This function also allocates 1024 tables on PDT[770] for frame_to_page
     * 	@details
     *  Page tables will be written in frames 0 to 1023. \n
     *	Page tables are virtually located on pages 1024*769 to 1024*769+1023. \n
     *	@note Any call to malloc will never return 0 on a successful allocation, as virtual address 0 maps the first page
     *	table, and will consequently never be returned by malloc if allocation is successful. \n
     *	NULL will then always be considered as an invalid address.
     *	@warning This function shall only be called once
     *	@return lowest free pte
     */
    uint allocate_page_tables();

    /** Mark the pages where GRUB module is loaded as allocated
     *
     * @param multibootInfo GRUB multiboot struct pointer
     */
    void load_grub_modules(const struct multiboot_info* multibootInfo);

    /**
     * Free pages in large free memory blocks
     */
    void free_release_pages(Process* process);

    /**
     * Zeroes out a memory region spanning over pages possibly lazily allocated.
     * This makes pages actually allocated to be zeroed out, without zeroing lazily allocated pages, which will be
     * zeroed anyway if they are accessed, by the page fault handling process.
     * @param total_size total size of the memory region to zero out
     * @param mem pointer to memory region
     * @param process process with relevant page tables
     */
    void zero_out_possibly_lazily_allocated_memory(uint total_size, void* mem, const Process* process);

    /**
     * Creates the process that will store global kernel memory mapping.
     * As malloc uses this process, new Process() cannot be called before such a process exists.
     * Thus, this function manually allocates some memory, then constructs by hand a process inside it.
     * Then, new Process() is called to properly construct the process (since new handles c++ initialization stuff
     * that a simple memory allocation doesn't)
     * @param lowest_free_pte lowest free page table entry
     */
    void create_kernel_process(uint lowest_free_pte);

    /** Handles a COW page fault that occurred in the kernel address space
     *
     * @param current_process process that was running when the page fault occurred
     * @param higher_half whether the page fault occurred in the higher half of the kernel address space
     * @param page_id page id that caused the fault
     * @param pt page table to use for the fault handling
     */
    void handle_shared_page_fault(const Process* current_process, bool higher_half, uint page_id, const page_table_t* pt);

    /** Handles a lazy zero page fault that occurred in the kernel address space
     *
     * @param current_process process that was running when the page fault occurred
     * @param higher_half whether the page fault occurred in the higher half of the kernel address space
     * @param page_id page id that caused the fault
     * @param pt page table to use for the fault handling
     */
    void handle_lazy_zero_page_fault(Process* current_process, bool higher_half, uint page_id, page_table_t* pt);

    uint get_free_frame()
    {
        while (lowest_free_frame < PDT_ENTRIES * PT_ENTRIES && FRAME_USED(lowest_free_frame))
            lowest_free_frame++;

        return lowest_free_frame;
    }

    uint get_free_pe()
    {
        while (kernel_process->lowest_free_pe < PDT_ENTRIES * PT_ENTRIES && PTE_USED(page_tables, kernel_process->lowest_free_pe))
            kernel_process->lowest_free_pe++;

        return kernel_process->lowest_free_pe;
    }

    uint get_free_pe_user()
    {
        while (lowest_free_pe_user < PDT_ENTRIES * PT_ENTRIES && PTE_USED(page_tables, lowest_free_pe_user))
            lowest_free_pe_user++;

        return lowest_free_pe_user;
    }

    void init_frame_rc()
    {
        // Allocate space for frame_rc
        for (size_t i = 0; i < PT_ENTRIES; i++)
        {
            PTE(page_tables, 771 * PDT_ENTRIES + i) = FRAME_ID_ADDR(PDT_ENTRIES * 3 + i) | PAGE_WRITE | PAGE_PRESENT;
            INVALIDATE_PAGE(771, i);
        }
        frame_rc = (uint*)VIRT_ADDR(771, 0, 0);

        memset(frame_rc, 0, 767 * PT_ENTRIES * sizeof(uint));
        memset(frame_rc + 771 * PT_ENTRIES, 0, (PDT_ENTRIES - 771) * PT_ENTRIES * sizeof(uint));

        // Fill frame_rc
        for (uint pde = 768; pde < 771; pde++)
        {
            uint frame_off = PDT_ENTRIES * pde;
            for (uint pte = 0; pte < PT_ENTRIES; pte++)
            {
                uint frame_id = frame_off + pte;
                frame_rc[frame_id] = PTE_USED(page_tables, frame_id);
            }
        }
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
        // Fill frame_to_page - page tables and frame_to_page and frame_rc
        for (size_t i = 0; i < PT_ENTRIES * 3; i++)
            frame_to_page[PT_ENTRIES + i] = 769 * PDT_ENTRIES + i;
    }

    uint allocate_page_tables()
    {
        // Save for later use
        uint asm_pt1_page_table_entry = ADDR_PTE((uint)asm_pt1);

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
        /** Clear unused page tables (all but 769 and 770 and 771)
         * Kernel code is for now referenced in PDT[768] entries, not in page_tables[768], so we can also clear it
        */
        memset(&page_tables[0].entries[0], 0, 769 * PT_ENTRIES * sizeof(uint));
        memset(&page_tables[772].entries[0], 0, (PDT_ENTRIES - 772) * PT_ENTRIES * sizeof(uint));

        // Copy asm_pt1 (the page table that maps the pages used by the kernel) in appropriated page table
        memcpy(&page_tables[768].entries[0], &asm_pt1->entries[0], PT_ENTRIES * sizeof(uint));

        // Register page tables in PDT
        for (int i = 0; i < PDT_ENTRIES; ++i)
            pdt->entries[i] = PTE_PHYS_ADDR(i) | PAGE_PRESENT | PAGE_WRITE;

        // Deallocate asm_pt1's page
        page_tables[768].entries[asm_pt1_page_table_entry] = 0;

        reload_cr3_asm(); // Apply changes | Full TLB flush is needed because we modified every pdt entry

        init_frame_to_page();
        init_frame_rc();

        uint used_page_directories = 4; // 0 is kernel, 1 is page tables, 2 is frame_to_page, 3 is frame_rc
        lowest_free_frame = PT_ENTRIES * used_page_directories;
        lowest_free_pe_user = 1; // 0 is reserved for page faulting

        return (768 + used_page_directories) * PDT_ENTRIES; // Kernel is only allowed to allocate in higher half
    }

    /*void load_grub_modules(const multiboot_info* multibootInfo)
    {
        grub_modules = (GRUB_module*)malloc(multibootInfo->mods_count * sizeof(GRUB_module));

        for (uint i = 0; i < multibootInfo->mods_count; ++i)
        {
            multiboot_module_t* module = &((multiboot_module_t*)(multibootInfo->mods_addr + KERNEL_VIRTUAL_BASE))[i];

            // Set module page as present
            uint module_start_frame_id = module->mod_start >> 12;
            uint mod_size = module->mod_end - module->mod_start;
            uint required_pages = (mod_size + PAGE_SIZE - 1) >> 12;

            uint pe_id = kernel_process->lowest_free_pe;

            grub_modules[i].start_addr = ((pe_id / PDT_ENTRIES) << 22) | ((pe_id % PDT_ENTRIES) << 12);
            grub_modules[i].size = mod_size;

            // Mark module's code pages as allocated
            for (uint j = 0; j < required_pages; ++j)
            {
                uint frame_id = module_start_frame_id + j;
                allocate_page<false>(frame_id, pe_id);

                pe_id++;
            }

            kernel_process->lowest_free_pe = pe_id;
        }

        loaded_grub_modules = multibootInfo->mods_count;
    }*/

    void create_kernel_process(uint lowest_free_pte)
    {
        // How many pages are necessary to store a process
        auto n_pages = (sizeof(Process) + PAGE_SIZE - 1) >> 12;

        if (n_pages > 1)
            irrecoverable_error("not handled because i'm lazy");

        void* mem = (void*)(lowest_free_pte << 12);

        // Allocate memory by hand to store the process
        for (size_t i = 0; i < n_pages; i++)
        {
            uint fr = get_free_frame();
            allocate_page<false>(fr, lowest_free_pte);
            while (PTE(page_tables, lowest_free_pte))
                lowest_free_pte++;
        }

        Scheduler::create_kernel_init_process(mem, lowest_free_pte, &kernel_process);

        // De-allocate memory
        for (size_t i = 0; i < n_pages; i++)
        {
            auto pha = PHYS_ADDR(page_tables, (uint)mem + (i << 12));
            auto pid = frame_to_page[pha >> 12];
            PTE(page_tables, pid)  = 0;
            __asm__ volatile("invlpg (%0)" : : "r" (pid << 12));
            MARK_FRAME_FREE((uint)(pha >> 12));
        }
    }

    void init(const multiboot_info* multiboot_info)
    {
        uint lfpe = allocate_page_tables();
        create_kernel_process(lfpe);
        //load_grub_modules(multibootInfo);
        Multiboot::init(register_multiboot_info(multiboot_info));
    }

    multiboot_info_t* register_multiboot_info(const multiboot_info* minfo)
    {
        uint uaddr = (uint)minfo;
        uint base = uaddr >> 12;
        uint page_id = get_free_pe();

        uint curr = frame_to_page[base];
        if (curr != (uint)-1)
            irrecoverable_error("Cannot map multiboot info");

        // ...
        if (PAGE_SIZE - (uaddr & (PAGE_SIZE - 1)) < sizeof(uint32_t))
            irrecoverable_error("Are u kidding me ?");

        // Allocate a page to get access to the multiboot struct size
        Memory::allocate_page<false>(base, page_id);
        uint page_addr = page_id << 12;
        uint tmp_v_addr = page_addr + (uaddr & (PAGE_SIZE - 1));
        auto total_size= ((multiboot_info_t*)tmp_v_addr)->total_size; // Get size
        // Deallocate first page
        free_page(page_addr, kernel_process);

        // Map the whole structure.
        multiboot_info_t* v_minfo;
        if (!((v_minfo = (multiboot_info_t*)register_physical_data(uaddr, total_size))))
            irrecoverable_error("Cannot map multiboot info");

        return v_minfo;
    }

    uint get_contiguous_pages(uint n, const Process* process)
    {
        uint b = process->lowest_free_pe;
        while (true)
        {
            // Maximum possibly free memory is too small to fulfill request
            if (b + n > PDT_ENTRIES * PT_ENTRIES)
                return (uint)-1;

            uint pte = b;
            uint target = b + n;

            // Explore contiguous free blocks while explored block size does not fulfill the request
            for (; !(PTE_USED(process->page_tables, pte)) && pte != target; pte++) {}

            // We have explored a free block that is big enough
            if (pte == target)
                return b;

            // There is a free virtual memory block from b to pte - 1 that is too small. Page entry pte - 1 is present.
            // Then next possibly free page entry is the (pte)th
            b = pte + 1;
        }
    }

    template<bool lazy_zero>
    void* sbrk(uint num_pages_requested, Process* process)
    {
        // Memory full
        if (lowest_free_frame == PT_ENTRIES * PDT_ENTRIES)
            return nullptr;

        uint b = get_contiguous_pages(num_pages_requested, process);
        if (b == (uint)-1)
            return nullptr;
        uint e = b + num_pages_requested; /* block end page index + 1*/

        // Allocate pages
        if (process->pdt == pdt) // Kernel process
        {
            for (uint i = b; i < e; ++i)
                allocate_page<lazy_zero>(i);
        }
        else
        {
            if constexpr (!lazy_zero)
            {
                // Allocate both in process and kernel page tables
                for (uint i = b; i < e; ++i)
                {
                    auto sys_pe = get_free_pe();
                    allocate_page_user<lazy_zero>(sys_pe);
                    process->update_pte(i, PTE(page_tables, sys_pe), true);
                }
            }
            else // Lazy allocation, do not allocate in kernel page tables
                for (uint i = b; i < e; ++i)
                    process->update_pte(i, PAGE_LAZY_ZERO | PAGE_USER, true);
        }

        while (PTE_USED(process->page_tables, process->lowest_free_pe))
            process->lowest_free_pe++;

        // Allocated memory block virtually starts at page b. Return it.
        return (void*)(b << 12);
    }

    template <bool lazy_zero>
    memory_header* more_kernel(uint n, Process* process)
    {
        if (n < N_ALLOC)
            n = N_ALLOC;

        uint requested_byte_size = n * sizeof(memory_header);
        auto num_pages_requested = (requested_byte_size + PAGE_SIZE - 1) >> 12;;
        auto* h = (memory_header*)sbrk<lazy_zero>(num_pages_requested, process);

        if ((int*)h == nullptr)
            return nullptr;

        uint allocated_byte_size = num_pages_requested * PAGE_SIZE;
        h->s.size = allocated_byte_size / sizeof(memory_header);

        uint free_bytes_save = process->free_bytes;
        process->free_bytes = -allocated_byte_size; // Prevent free from freeing pages we just allocated
        free((void*)(h + 1), process);
        process->free_bytes = free_bytes_save + allocated_byte_size;

        return process->freep;
    }

    void free_release_pages(Process* process)
    {
        memory_header* p = process->freep;
        for (memory_header* c = process->freep->s.ptr;; p = c, c = c->s.ptr)
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
                if (c == process->freep)
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
            c = process->freep = p;

            // Free pages
            for (uint i = 0; i < n_pages; ++i)
            {
                uint aligned_addr = aligned_addr_base + (i << 12);
                free_page(aligned_addr, process);
            }

            process->free_bytes -= aligned_free_bytes; // Update free_bytes
        }

        // printf("\n====\n");
    }

    template<bool lazy_zero>
    void allocate_page(uint frame_id, uint page_id)
    {
        // Write PTE
        PTE(page_tables, page_id) = FRAME_ID_ADDR(frame_id) | (lazy_zero ? PAGE_LAZY_ZERO : PAGE_PRESENT) | PAGE_WRITE;
        __asm__ volatile("invlpg (%0)" : : "r" (frame_id << 12));

        if constexpr (!lazy_zero)
            MARK_FRAME_USED(frame_id, page_id);
    }

    template<bool lazy_zero>
    void allocate_page(uint page_id)
    {
        uint frame = 0;
        if constexpr (!lazy_zero)
            frame = get_free_frame();
        allocate_page<lazy_zero>(frame, page_id);
    }

    void free_page(uint address, const Process* process)
    {
        // Cache is not updated, since this is not necessary on free. Pages may appear allocated (from the CPU's
        // perspective) even though they are not, but this is not an issue, as long as cache is updated upon allocation

        uint pde = ADDR_PDE(address);
        uint pte = ADDR_PTE(address);
        uint sys_page_id, frame_id;
        if (process->pdt != pdt)
        {
            uint page_id = address >> 12;

            auto entry = process->page_tables[pde].entries[pte];
            if (entry & PAGE_LAZY_ZERO)
            {
                // We are freeing a page which has been lazily allocated and never accessed, thus there
                // is no frame to free and no associated kernel page table entry.
                // We can simply zero out the entry and exit.
                process->update_pte(page_id, 0, false);
                return;
            }

            // Compute sys_page_id using physical address and frame_to_page
            auto physical_address = PHYS_ADDR(process->page_tables, address);
            frame_id = physical_address >> 12;
            sys_page_id = frame_to_page[frame_id];
            process->update_pte(page_id, 0, false); // Update process pte
        }
        else
        {
            // Current process is kernel process, so index computation is straightforward
            sys_page_id = pde * PT_ENTRIES + pte;
            frame_id = PHYS_ADDR(page_tables, address) >> 12;
        }

        uint rc = frame_rc[frame_id];
        if (rc > 1) // Frame is still used, do not free it
            return;

        // Write PTE in kernel global memory mapping
        auto pte_ptr = &PTE(page_tables, sys_page_id);
        bool lazy = *pte_ptr & PAGE_LAZY_ZERO;
        if (!lazy && frame_id == 0)
            irrecoverable_error("wut");
        *pte_ptr = 0;

        if (!lazy)
            MARK_FRAME_FREE(frame_id); // Internal deallocation registration
    }

    template<bool lazy_zero>
    void allocate_page_user(uint frame_id, uint page_id)
    {
        allocate_page<lazy_zero>(frame_id, page_id);
        PTE(page_tables, page_id) |= PAGE_USER;
        // Todo: invalidate cache correctly
    }

    template<bool lazy_zero>
    void allocate_page_user(uint page_id)
    {
        uint frame = 0;
        if constexpr (!lazy_zero)
            frame = get_free_frame();
        allocate_page_user<lazy_zero>(frame, page_id);
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

    void* calloca(size_t nmemb, size_t size)
    {
        uint base_size = size;
        if (__builtin_mul_overflow(nmemb, size, &base_size))
            return nullptr;
        uint total_size;
        uint additional_size = sizeof(void*) + PAGE_SIZE;
        if (__builtin_add_overflow(base_size, additional_size, &total_size))
            return nullptr;

        void* base_addr = calloc(1, total_size, kernel_process);
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
        uint page_id = ADDR_PAGE(addr);
        uint num_pages = (size + PAGE_SIZE - 1) >> 12;
        for (uint i = 0; i < num_pages; ++i)
        {
            if (PTE_USED(page_tables, page_id) || FRAME_USED(page_id))
                return false;
            page_id++;
        }

        page_id = ADDR_PAGE(addr);
        for (uint i = 0; i < num_pages; ++i)
        {
            allocate_page<false>(page_id, page_id);
            page_id++;
        }

        return true;
    }

    void* physically_aligned_malloc(uint n)
    {
        uint page_beg = (uint)-1;
        uint frame_beg = (uint)-1;
        uint num_pages = (n + PAGE_SIZE - 1) >> 12;
        for (auto p = kernel_process->lowest_free_pe; p < PDT_ENTRIES * PT_ENTRIES; p++)
        {
            uint i;
            for (i = 0; i < num_pages && !PTE_USED(page_tables, p + i); i++)
            {
            };
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
            for (i = 0; i < num_pages && !FRAME_USED(p + i); i++)
            {
            };
            if (i == num_pages)
            {
                frame_beg = p;
                break;
            }
        }
        if (frame_beg == (uint)-1)
            return nullptr;

        for (uint i = 0; i < num_pages; ++i)
            allocate_page<false>(frame_beg + i, page_beg + i);

        return (void*)(page_beg << 12);
    }

    void handle_shared_page_fault(const Process* current_process, bool higher_half, uint page_id, const page_table_t* pt)
    {
        if (current_process->pdt == pdt)
            irrecoverable_error("COW on kernel process not supposed to happen");
        if (higher_half)
            irrecoverable_error("COW on higher half");

        uint sys_pe = get_free_pe_user(); // Get sys PTE id
        uint frame = get_free_frame(); // Get frame id
        Memory::allocate_page_user<false>(frame, sys_pe); // Allocate page in kernel address space
        auto pte_flags = ((PTE(pt, page_id) & 0x7FF) & ~PAGE_COW) | PAGE_WRITE; // Flags to use for new page
        uint mapping_pe = get_contiguous_pages(1, current_process); // Get a free pe

        // Move old page to mapping page
        current_process->update_pte(mapping_pe, PTE(pt, page_id), true);
        // Update page table entry to point to new page
        current_process->update_pte(page_id, FRAME_ID_ADDR(frame) | pte_flags, true);

        // Copy page to new page
        memcpy((void*)(page_id << 12), (void*)(mapping_pe << 12), PAGE_SIZE);

        // Unmap old page which is now at mapping_pe
        current_process->update_pte(mapping_pe, 0, true);

        // If the frame of the original page is not used anymore, free it
        if (frame_rc[frame] == 0)
            MARK_FRAME_FREE(frame);
    }

    void handle_lazy_zero_page_fault(Process* current_process, bool higher_half, uint page_id, page_table_t* pt)
    {
        // Allocate frame and update memory mapping
        auto pte_ptr = &PTE(pt, page_id); // Get pointer to pte
        bool page_user = *pte_ptr & PAGE_USER; // Should page be user accessible ?
        uint frame_id = get_free_frame(); // Get frame
        *pte_ptr = FRAME_ID_ADDR(frame_id) | (page_user ? PAGE_USER : 0) | PAGE_WRITE | PAGE_PRESENT; // Update pte
        INVALIDATE_PAGE(page_id >> 10, page_id & 0x3FF); // Invalidate cache
        memset((void*)(page_id << 12), 0, PAGE_SIZE); // Zero out page

        // If process is kernel process or address is in higher half, kernel global page tables have already
        // been updated, we just need to register the allocated frame
        if (current_process->pdt == pdt || higher_half)
        {
            MARK_FRAME_USED(frame_id, page_id);
        }
        else
        {
            // Register allocation of the frame in kernel global page tables
            uint sys_page_id = get_free_pe();
            PTE(page_tables, sys_page_id) = *pte_ptr;
            MARK_FRAME_USED(frame_id, sys_page_id);
        }
        // If the process is not the kernel, then the page is referenced by the kernel AND the process, thus we need
        // to increment the frame reference count by one
        // Normally this is done in Process::update_pte, but here we are not using it, this could be changed. Mind
        // that this would then trigger an error in MARK_FRAME_USED since we would be trying to allocate a frame
        // which has a non-null ref count.
        if (current_process->pdt != pdt)
            frame_rc[frame_id]++;
    }

    bool page_fault_handler(Process* current_process, uint fault_address, bool write_access)
    {
        // Gather information
        auto page_id = ADDR_PAGE(fault_address);
        bool higher_half = fault_address >= KERNEL_VIRTUAL_BASE;
        auto pt = higher_half ? page_tables : current_process->page_tables;
        auto pte = PTE(pt, page_id);

        bool handled = false;

        // Handle Shared Read-Only (SHRO) and Copy-On-Write (COW) pages
        if (pte & PAGE_SHRO)
        {
            if (write_access)
                return false;
            handle_shared_page_fault(current_process, higher_half, page_id, pt);
            handled = true;
        }
        else if (pte & PAGE_COW)
        {
            handle_shared_page_fault(current_process, higher_half, page_id, pt);
            handled = true;
        }

        // Check whether relevant page has been lazily allocated
        if (!(pte && pte & PAGE_LAZY_ZERO))
            return handled;

        handle_lazy_zero_page_fault(current_process, higher_half, page_id, pt);

        return true;
    }

    void* register_physical_data(uint physical_address, uint size)
    {
        auto n_pages = (size + (physical_address & (PAGE_SIZE - 1)) + PAGE_SIZE - 1) >> 12;
        auto frame_base = physical_address >> 12;

        bool all_frame_used = true;
        bool no_frame_used = true;
        for (uint i = 0; i < n_pages; i++)
        {
            bool used = FRAME_USED(frame_base + i);
            all_frame_used &= used;
            no_frame_used &= !used;
        }

        // No frame over the required block is referenced, so will then try to find enough contiguous page table entries
        // to map the whole memoery block
        if (no_frame_used)
        {
            uint b = kernel_process->lowest_free_pe; /* block beginning page index */

            // Try to find contiguous free virtual block of memory
            while (true)
            {
                // Maximum possibly free memory is too small to fulfill request
                if (b + n_pages > PDT_ENTRIES * PT_ENTRIES)
                    return nullptr;

                uint pte = b;
                uint target = b + n_pages;

                // Explore contiguous free blocks while explored block size does not fulfill the request
                for (; !(PTE_USED(kernel_process->page_tables, pte)) && pte != target; pte++)
                {
                }

                // We have explored a free block that is big enough
                if (pte == target)
                    break;

                // There is a free virtual memory block from b to pte - 1 that is too small. Page entry pte - 1 is present.
                // Then next possibly free page entry is the (pte)th
                b = pte + 1;
            }

            for (uint i = 0; i < n_pages; i++)
                allocate_page<false>(frame_base + i, b + i);

            return (void*)((b << 12) + (physical_address & (PAGE_SIZE - 1)));
        }

        // Everything is already mapped, simply return the corresponding virtual address
        if (all_frame_used)
            return (void*)((frame_to_page[physical_address >> 12] << 12) + (physical_address & (PAGE_SIZE - 1)));

        return nullptr;
    }

    void zero_out_possibly_lazily_allocated_memory(uint total_size, void* mem, const Process* process)
    {
        auto n_pages = (total_size + PAGE_SIZE - 1) >> 12; // Number of pages allocation spans over
        auto pt = process->page_tables; // Page tables to use
        uint base_page_id = ADDR_PAGE((uint)mem); // Page id of first page
        uint bytes_onf_first_page = PAGE_SIZE - ((uint)mem & (PAGE_SIZE - 1)); // How many bytes are on the first page
        bytes_onf_first_page = bytes_onf_first_page > total_size ? total_size : bytes_onf_first_page; // Cap to total_size
        uint rem = total_size; // Remaining bytes to zero out

        // Handle first page alone since start address is not necessarily page aligned
        if (PTE(pt, base_page_id) & PAGE_PRESENT)
            memset(mem, 0, bytes_onf_first_page);

        // Handle other pages
        for (size_t i = 1; i < n_pages; i++)
        {
            if (PTE(pt, base_page_id + i) & PAGE_PRESENT)
            {
                uint n = rem & (PAGE_SIZE - 1); // How many bytes are on this page
                uint address = (base_page_id + i) << 12; // Address of the beginning of the page
                memset((void*)address, 0, n); // Zero out
                rem -= n; // Update remaining byte count
            }
        }
    }
}

using namespace Memory;

void* operator new(size_t size)
{
    return malloc<false>(size, kernel_process);
}

void* operator new[](size_t size)
{
    return malloc<false>(size, kernel_process);
}

void operator delete(void* p)
{
    free(p, kernel_process);
}

void operator delete[](void* p)
{
    free(p, kernel_process);
}

void operator delete(void* p, [[maybe_unused]] long unsigned int size)
{
    free(p, kernel_process);
}

void operator delete[]([[maybe_unused]] void* p, [[maybe_unused]] long unsigned int size)
{
    irrecoverable_error("this operator shouldn't be used");
}

void operator delete(void* p, [[maybe_unused]] unsigned int n)
{
    free(p, kernel_process);
}

void operator delete [](void* p, [[maybe_unused]] unsigned int n)
{
    free(p, kernel_process);
}

template<bool lazy_zero>
void* malloc(uint n, Process* process)
{
    if (!n)
        return nullptr;

    memory_header* c;
    memory_header* p = process->freep;
    unsigned nunits = (n + sizeof(memory_header) - 1) / sizeof(memory_header) + 1;

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
            process->free_bytes -= nunits * sizeof(memory_header);
            process->freep = p;
            return (void*)(c + 1);
        }
        if (c == process->freep) /* wrapped around free list */
        {
            if ((c = more_kernel<lazy_zero>(nunits, process)) == nullptr)
                return nullptr; /* none left */
        }
    }
}

extern "C" void* malloc(uint n)
{
    return malloc<false>(n, kernel_process);
}

void* calloc(size_t nmemb, size_t size, Process* process)
{
    // Check edge cases according to man page
    if (!nmemb || !size)
        return malloc<false>(1, process);

    // Check for overflow
    size_t total_size;
    if (__builtin_mul_overflow(nmemb, size, &total_size))
        return nullptr;

    // Get memory
    void* mem = malloc<true>(total_size, process);
    if (!mem)
        return nullptr;

    // Zero out memory which isn't in lazily allocated pages
    zero_out_possibly_lazily_allocated_memory(total_size, mem, process);

    return mem;
}

extern "C" void* calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size, kernel_process);
}

void free(void* ptr, Process* process)
{
    if (ptr == nullptr)
        return;

    memory_header* c = (memory_header*)ptr - 1;
    memory_header* p;
    process->free_bytes += c->s.size * sizeof(memory_header);

    // Loop until p < c < p->s.ptr
    for (p = process->freep; !(c > p && c < p->s.ptr); p = p->s.ptr)
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
    process->freep = p;

    // Free pages
    if (process->free_bytes >= FREE_THRESHOLD)
        free_release_pages(process);
}

extern "C" void free(void* ptr)
{
    free(ptr, kernel_process);
}

void* realloc(void* ptr, size_t size, Process* process)
{
    if (!ptr) // realloc on null = malloc
        return malloc<false>(size, process);
    if (!size) // realloc with size 0 = free
    {
        free(ptr, process);
        return nullptr;
    }

    // Get header
    memory_header* h = (memory_header*)ptr - 1;

    // Block is big enough
    if ((h->s.size - 1) * sizeof(memory_header) >= size) // (-1 because size accounts for the memory header itself)
        return ptr;

    memory_header* p; // previous header

    // Loop until p < h < p->s.ptr
    for (p = process->freep; !(h > p && h < p->s.ptr); p = p->s.ptr)
        //Break when arrived at the end of the list and c goes before beginning or after end
        if (p >= p->s.ptr && (h < p->s.ptr || h > p))
            break;

    memory_header* next = p->s.ptr;
    auto block_content_byte_size = (h->s.size - 1) * sizeof(memory_header);
    // Check if there is a contiguous free block whose combined size if large enough to contain 'size' bytes
    if (h + h->s.size == next && block_content_byte_size + next->s.size * sizeof(memory_header) >= size)
    {
        h->s.size += next->s.size; // Add size of contiguous free block in current block

        // Update free block chain
        p->s.ptr = next->s.ptr;
        process->freep = p;

        return ptr;
    }

    // We have no choice left but to copy data in a newly allocated buffer and free original data
    void* new_mem = malloc<false>(size, process);

    if (!new_mem)
        return ptr; // If realloc fails, the man page indicates it should return the unchanged original buffer

    memcpy(new_mem, ptr, block_content_byte_size);
    free(ptr, process);

    return new_mem;
}


void* realloc(void* ptr, size_t size)
{
    return realloc(ptr, size, kernel_process);
}