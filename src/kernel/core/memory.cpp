#include "memory.h"
using namespace Memory;

#include "fb.h"
#include <kstring.h>
#include <stdint.h>
#include <sys/mman.h>

#include "../processes/scheduler.h"
#include "../utils/comparison.h"
#include "abi-bits/errno.h"
#include "RawMemory.h"

namespace Memory
{
    Process* kernel_process = nullptr;

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

    /** Allocate 1024 pages to store the 1024 pages tables required to map all the memory in PDT[769]. \n
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
    void handle_cow_page_fault(const Process* current_process, bool higher_half, uint page_id, const page_table_t* pt);

    /** Handles a lazy zero page fault that occurred in the kernel address space
     *
     * @param current_process process that was running when the page fault occurred
     * @param higher_half whether the page fault occurred in the higher half of the kernel address space
     * @param page_id page id that caused the fault
     * @param pt page table to use for the fault handling
     */
    void handle_lazy_zero_page_fault(Process* current_process, bool higher_half, uint page_id, page_table_t* pt);

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
            allocate_page(fr, lowest_free_pte, DEFAULT_K_POLICY);
            while (PTE(page_tables, lowest_free_pte))
                lowest_free_pte++;
        }

        Scheduler::create_kernel_init_process(mem, lowest_free_pte, &kernel_process);

        // Do not free as kernel_process now lives inside those pages
    }

    void init(const multiboot_info* multiboot_info)
    {
        uint lfpe = allocate_page_tables();
        create_kernel_process(lfpe);
        //load_grub_modules(multibootInfo);

        // Todo: register allocations in kernel process
        // Todo: make sure all allocating functions reference the allocations in kernel memtree

        Multiboot::init(register_multiboot_info(multiboot_info));
        if (multiboot_info == nullptr)
        {
            uint fb_info_addr = phys_to_virt_addr(FB_MODE_INFO_ADDR_WHEN_CUSTOM_BOOTLOADER_USED);
            if (fb_info_addr == (uint)-1)
                register_physical_data(FB_MODE_INFO_ADDR_WHEN_CUSTOM_BOOTLOADER_USED, sizeof(FB::vbe_mode_info_structure));
        }
    }

    multiboot_info_t* register_multiboot_info(const multiboot_info* minfo)
    {
        if (minfo == nullptr)
            return nullptr;

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
        Memory::allocate_page(base, page_id, DEFAULT_K_POLICY);
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

    uint get_contiguous_pages(uint n, const hint_info& hint_info, const Process* process)
    {
        return get_contiguous_pages(n, hint_info, process->page_tables, process->lowest_free_pe);
    }

    uint phys_to_virt_addr(uint phys_addr)
    {
        uint frame_id = phys_addr >> 12;
        if (frame_to_page[frame_id] == (uint)-1)
            return (uint)-1;

        return (frame_to_page[frame_id] << 12) + (phys_addr & (PAGE_SIZE - 1));
    }

    int mprotect(void* addr, size_t len, int prot, const Process* process)
    {
        const auto uaddr = (Elf32_Addr)addr;
        if (uaddr & (PAGE_SIZE - 1))
            return -EINVAL;

        constexpr int supported_flags = PROT_READ | PROT_WRITE | PROT_EXEC;
        if (prot & ~supported_flags)
        {
            printf_warn("mprotect called with the following unsupported flags: 0x%x. Failing.", prot & ~supported_flags);
            return -EINVAL;
        }

        // Check address range is valid within the process address space
        const auto num_pages = ADDR_PAGE(len + PAGE_SIZE - 1);
        bool range_is_valid = true;
        for (uint id = 0; id < num_pages; id++)
        {
            uint page_id = ADDR_PAGE(uaddr + id);
            uint pte = PTE(process->page_tables, page_id);
            if (!(pte & PAGE_PRESENT || pte & PAGE_LAZY_ZERO))
            {
                if (pte & PAGE_COW || pte & PAGE_SHRO)
                    irrecoverable_error("mprotect: address range covers a COW or SHRO page, this is not supported yet");
                range_is_valid = false;
                break;
            }
        }
        if (!range_is_valid)
            return -ENOMEM; // Strange error code imo, but that is what the man page says

        // For now the only flag that can be affected by mprotect is PAGE_WRITE
        const uint new_flags = prot & PROT_WRITE ? PAGE_WRITE : 0;

        // Apply new flags
        for (uint id = 0; id < num_pages; id++)
        {
            constexpr uint flags_mask = ~PAGE_WRITE;

            uint page_id = ADDR_PAGE(uaddr + id);
            uint pte = PTE(process->page_tables, page_id);
            pte &= flags_mask;
            pte |= new_flags;
            process->update_pte(page_id, pte, true);
        }

        return 0;
    }

    void* sbrk(uint num_pages_requested, const page_info& page_info, const hint_info& hint_info, Process* process)
    {
        // Memory full
        if (lowest_free_frame == PT_ENTRIES * PDT_ENTRIES)
            return nullptr;

        uint b = get_contiguous_pages(num_pages_requested, hint_info, process);
        if (b == (uint)-1)
            return nullptr;
        uint e = b + num_pages_requested; /* block end page index + 1*/

        // Allocate pages
        if (constexpr int supported_policy = PAGE_USER | PAGE_LAZY_ZERO | PAGE_PRESENT | PAGE_WRITE; page_info.policy & ~supported_policy)
            irrecoverable_error("sbrk: Unsupported allocation flags: 0x%x", page_info.policy & ~supported_policy);
        if ((page_info.policy & PAGE_PRESENT) && (page_info.policy & PAGE_LAZY_ZERO))
            irrecoverable_error("sbrk: both PAGE_PRESENT and PAGE_LAZY zero cannot be specified at the same time");

        if (process->pdt == pdt) // Kernel process
        {
            for (uint i = b; i < e; ++i)
                allocate_page(i, page_info.policy);
        }
        else
        {
            if (!(page_info.policy & PAGE_USER))
                irrecoverable_error("sbrk: allocating pages for user process without PAGE_USER flag");
            if (!(page_info.policy & PAGE_LAZY_ZERO))
            {
                // Allocate both in process and kernel page tables
                for (uint i = b; i < e; ++i)
                {
                    // Todo: get rid of kernel allocation which is a useless duplicate
                    // This will certainly have impacts on frame_rc...
                    const uint sys_pe = get_free_pe();
                    allocate_page(sys_pe, page_info.policy);
                    process->update_pte(i, PTE(page_tables, sys_pe), true);
                }
            }
            else // Lazy allocation, do not allocate in kernel page tables
                for (uint i = b; i < e; ++i)
                    process->update_pte(i, page_info.policy, true);
        }

        while (PTE_USED(process->page_tables, process->lowest_free_pe))
            process->lowest_free_pe++;

        // Allocated memory block virtually starts at page b. Return it.
        return (void*)(b << 12);
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

            const auto entry = process->page_tables[pde].entries[pte];
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
            frame_rc[frame_id]--; // Decrement rc. For user processes this is done in udpate_pte
        }

        uint rc = frame_rc[frame_id];
        if (rc >= 1) // Frame is still used, do not free it
            return;

        free_page(sys_page_id);
    }

    void* malloca(uint size)
    {
        const uint num_pages = ADDR_PAGE(size + PAGE_SIZE - 1);
        int err;
        return mmap((void*)KERNEL_VIRTUAL_BASE, num_pages * size, DEFAULT_K_PROT, DEFAULT_K_FLAGS, DEFAULT_K_POLICY, 0, err, kernel_process, false, false);
    }

    void* calloca(size_t nmemb, size_t size)
    {
        uint base_size = size;
        if (__builtin_mul_overflow(nmemb, size, &base_size))
            return nullptr;
        uint total_size;
        const uint num_pages = ADDR_PAGE(base_size + PAGE_SIZE - 1);
        const uint new_size = num_pages * PAGE_SIZE;
        const uint additional_size = new_size - base_size;
        if (__builtin_add_overflow(base_size, additional_size, &total_size))
            return nullptr;

        int err;
        void* alloc = mmap((void*)KERNEL_VIRTUAL_BASE, num_pages * size, DEFAULT_K_PROT, DEFAULT_K_FLAGS, PAGE_WRITE | PAGE_LAZY_ZERO, 0, err, kernel_process, false, false);
        if (!alloc)
            return nullptr;
        zero_out_possibly_lazily_allocated_memory(new_size, alloc, kernel_process);
        return alloc;
    }

    void freea(void* ptr)
    {
        free(ptr);
    }

    uint* get_stack_top_ptr()
    {
        return stack_top_ptr;
    }

    void* mmap(void* hint, size_t size, int prot, int flags, [[maybe_unused]] int fd, off_t offset, int& err, Process* process, bool
               lazy_zero, bool page_user)
    {
#define mmap_ret_err(error) {err = error; return nullptr;}
#define mmap_ret_err_with_warnv(error, msg, ...) {printf_warn(msg, __VA_ARGS__); err = error; return nullptr;}
#define mmap_ret_err_with_warn(error, msg) {printf_warn(msg); err = error; return nullptr;}

        constexpr int supported_flags = MAP_ANON | MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED;
        if (flags & ~supported_flags)
            mmap_ret_err_with_warnv(EINVAL, "mmap called with the following unsupported flags: 0x%x", flags & ~supported_flags)

        if (!((flags & MAP_PRIVATE) | (flags & MAP_SHARED)))
            mmap_ret_err(EINVAL)  // According to man page
        if ((Elf32_Addr)hint & (PAGE_SIZE - 1) || size & (PAGE_SIZE - 1) || offset & (PAGE_SIZE - 1))
            mmap_ret_err(EINVAL); // According to man page

        // When memory mapped files will be implemented, don't forget to handle EACCES error or mprotect (cf man page)

        // At that point we know that flags contain MAP_PRIVATE, so this case only is handled here for now
        // Beware that changing code above may require adding more conditional logic below

        // ReSharper disable once CppIdenticalOperandsInBinaryExpression
        if (!((flags & MAP_ANONYMOUS) | (flags & MAP_ANON)))
            mmap_ret_err_with_warn(EINVAL, "mmap called with MAP PRIVATE without MAP_ANONYMOUS or MAP_ANON. This is not"
                               "supported yet")

        if (prot == 0)
            mmap_ret_err_with_warn(EINVAL, "mmap called with prot == 0. I don't know how to implement that, returning failure")

        // At that point we know we have ANONYMOUS and MAP_PRIVATE

        // If hint is KERNEL_VIRTUAL_BASE, then there is obviously no free memory below kernel_process->lowest_free_pe << 12
        // So hint is set to that value to avoid useless searching
        // Note that passing KERNEL_VIRTUAL_BASE as hint is a way to force mmap to allocate in higher half, thus making
        // the allocation accessible from any address space
        if (hint == (void*)KERNEL_VIRTUAL_BASE)
            hint = (void*)(kernel_process->lowest_free_pe << 12);

        const int policy = (prot & PROT_WRITE ? PAGE_WRITE : 0) | (lazy_zero ? PAGE_LAZY_ZERO : PAGE_PRESENT) | (page_user ? PAGE_USER : 0);
        const hint_info hint_info{reinterpret_cast<uintptr_t>(hint), (bool)(flags & MAP_FIXED)};
        const page_info page_info{flags, policy};
        // printf_info("mmap call on range [0x%08x, 0x%08x]", (uint)hint, (uint)hint + (uint)size);
        void* window = process->memtree.allocate(size, page_info, process, hint_info);
        if (flags & MAP_FIXED && window != hint)
            irrecoverable_error("%s: MAP_FIXED set, but returned window does not match hit", __func__);
        if (!window)
            mmap_ret_err(ENOMEM);
        return window;
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
            allocate_page(page_id, page_id, DEFAULT_K_POLICY);
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
            allocate_page(frame_beg + i, page_beg + i, DEFAULT_K_POLICY);

        return (void*)(page_beg << 12);
    }

    void handle_cow_page_fault(const Process* current_process, bool higher_half, uint page_id, const page_table_t* pt)
    {
        if (current_process->pdt == pdt)
            irrecoverable_error("COW on kernel process not supposed to happen");
        if (higher_half)
            irrecoverable_error("COW on higher half");

        uint sys_pe = get_free_pe_user(); // Get sys PTE id
        uint frame = get_free_frame(); // Get frame id
        const uint current_policy = PTE(pt, page_id) & 0x7FF;
        const uint new_policy = (current_policy & ~PAGE_COW) | PAGE_WRITE;
        allocate_page(frame, sys_pe, new_policy); // Allocate page in kernel address space
        uint mapping_pe = get_contiguous_pages(1, DEFAULT_HINT_INFO, current_process); // Get a free pe

        // Move old page to mapping page
        current_process->update_pte(mapping_pe, PTE(pt, page_id), true);
        // Update page table entry to point to new page
        current_process->update_pte(page_id, FRAME_ID_ADDR(frame) | new_policy, true);

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
        const uint page_id = ADDR_PAGE(fault_address);
        const bool higher_half = fault_address >= KERNEL_VIRTUAL_BASE;
        page_table_t* pt = higher_half ? page_tables : current_process->page_tables;
        const uint pte = PTE(pt, page_id);

        bool handled = false;

        // Handle Copy-On-Write (COW) pages
        if (pte & PAGE_COW)
        {
            if (!write_access)
                irrecoverable_error("huh ??");
            handle_cow_page_fault(current_process, higher_half, page_id, pt);
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
        // to map the whole memory block
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
                allocate_page(frame_base + i, b + i, DEFAULT_K_POLICY);

            return (void*)((b << 12) + (physical_address & (PAGE_SIZE - 1)));
        }

        // Everything is already mapped, simply return the corresponding virtual address
        if (all_frame_used)
            return (void*)((frame_to_page[physical_address >> 12] << 12) + (physical_address & (PAGE_SIZE - 1)));

        return nullptr;
    }

    void zero_out_possibly_lazily_allocated_memory(uint total_size, void* mem, const Process* process)
    {
        const uintptr_t uaddr = reinterpret_cast<uintptr_t>(mem);
        const page_table_t* pt = process->page_tables; // Page tables to use
        const uint first_page_id = ADDR_PAGE(uaddr);
        const uintptr_t mem_off = uaddr & (PAGE_SIZE - 1);
        const uint bytes_on_first_page = min(PAGE_SIZE - mem_off, total_size);

        // Handle first page alone since start address is not necessarily page aligned
        if (PTE(pt, first_page_id) & PAGE_PRESENT)
            memset(mem, 0, bytes_on_first_page);

        // Handle other pages
        const uint num_remaining_pages = ADDR_PAGE(total_size - bytes_on_first_page + PAGE_SIZE - 1);
        uint rem = total_size - bytes_on_first_page; // Remaining bytes to zero out
        for (size_t i = 0; i < num_remaining_pages; i++)
        {
            const uint n = min(rem, (uint)PAGE_SIZE); // How many bytes are on this page
            if (PTE(pt, first_page_id + 1 + i) & PAGE_PRESENT)
            {
                const uint address = (first_page_id + 1 + i) << 12; // Address of the beginning of the page
                memset((void*)address, 0, n); // Zero out
            }
            rem -= n; // Update remaining byte count
        }
    }
}

void* operator new(size_t, void* p)
{
    return p;
}

void operator delete(void*, void*)
{
}

void* operator new(size_t size)
{
    return malloc(size, DEFAULT_K_PAGE_INFO, kernel_process);
}

void* operator new[](size_t size)
{
    return malloc(size, DEFAULT_K_PAGE_INFO, kernel_process);
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

void* malloc(uint n, const page_info& page_info, Process* process)
{
    if (!n)
        return nullptr;

    MemTree& mem_tree = process->memtree;
    return mem_tree.allocate(n, page_info, process);
}

extern "C" void* malloc(uint n)
{
    void* addr = malloc(n, DEFAULT_K_PAGE_INFO, kernel_process);
    if (!addr)
        printf_warn("malloc returned null");

    return addr;
}

void* calloc(size_t nmemb, size_t size, const page_info& page_info, Process* process)
{
    MemTree& mem_tree = process->memtree;

    // Check edge cases according to man page
    if (!nmemb || !size)
        return mem_tree.allocate(1, page_info, process);

    // Check for overflow
    size_t total_size;
    if (__builtin_mul_overflow(nmemb, size, &total_size))
        return nullptr;

    // Get memory
    void* mem = mem_tree.allocate(total_size, page_info, process);
    if (!mem)
        return nullptr;

    // Zero out memory which isn't in lazily allocated pages
    zero_out_possibly_lazily_allocated_memory(total_size, mem, process);

    return mem;
}

extern "C" void* calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size, DEFAULT_K_PAGE_INFO, kernel_process);
}

MemTree::FreeState free(void* ptr, Process* process)
{
    MemTree& mem_tree = process->memtree;
    return mem_tree.free(reinterpret_cast<uintptr_t>(ptr), process);
}

extern "C" void free(void* ptr)
{
    if (free(ptr, kernel_process) != MemTree::FreeState::OK)
        irrecoverable_error("free failed");
}

MemTree::ReallocState realloc(void* ptr, size_t size, Process* process, void*& new_address)
{
    MemTree& memtree = process->memtree;
    return memtree.realloc(reinterpret_cast<uintptr_t>(ptr), size, process, reinterpret_cast<uintptr_t&>(new_address));
}


void* realloc(void* ptr, size_t size)
{
    void* new_address;
    // ReSharper disable once CppDFAMemoryLeak
    if (realloc(ptr, size, kernel_process, new_address) != MemTree::ReallocState::OK)
        irrecoverable_error("realloc failed");
    return new_address;
}

void* lazy_malloc(uint n)
{
    constexpr page_info page_info{DEFAULT_K_FLAGS, PAGE_LAZY_ZERO | PAGE_WRITE};
    return malloc(n, page_info, kernel_process);
}
