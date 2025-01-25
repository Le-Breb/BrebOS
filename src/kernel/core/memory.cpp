#include "memory.h"

#include "fb.h"
#include "kstring.h"
#include "../file_management/VFS.h"

#define INVALIDATE_PAGE(pde, pte) __asm__ volatile("invlpg (%0)" : : "r" (VIRT_ADDR(pde, pte, 0)));

extern "C" void boot_page_directory();

extern "C" void boot_page_table1();

pdt_t* pdt = (pdt_t*)boot_page_directory;
page_table_t* asm_pt1 = (page_table_t*)boot_page_table1;
page_table_t* page_tables;

// Map of used frames. 1 = used, 0 = free. Ith page is referenced at (i % 32)th lsb of map[i / 32]
uint frame_bitmap[(PT_ENTRIES / 32) * PDT_ENTRIES];
uint lowest_free_pe;
uint lowest_free_frame = 0;

memory_header base = {.s = {&base, 0}};
memory_header* freep; /* Lowest free block */

[[maybe_unused]] uint loaded_grub_modules = 0;
GRUB_module* grub_modules;

uint free_bytes = 0;

extern "C" void stack_top();

uint* stack_top_ptr = (uint*)stack_top;

/** Tries to allocate a contiguous block of memory
 * @param n Size of the block in bytes
 *  @return A pointer to a contiguous block of n bytes or NULL if memory is full
 */
void* sbrk(uint n);

void init_page_bitmap();

/** Tries to allocate a contiguous block of (virtual) memory
 *
 * @param n Size to allocate. Unit: sizeof(mem_header)
 * @return Pointer to allocated memory if allocation was successful, NULL otherwise
 */
memory_header* more_kernel(uint n);

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
void free_release_pages();

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
	printf_error("this operator shouldn't be used");
}

void operator delete([[maybe_unused]] void* p, [[maybe_unused]] unsigned int n)
{
  	free(p);
}

void operator delete []([[maybe_unused]] void* p, [[maybe_unused]] unsigned int n)
{
	free(p);
}

uint get_free_page()
{
	uint page = lowest_free_frame;
	uint i = lowest_free_frame + 1;
	while (i < PDT_ENTRIES * PT_ENTRIES && FRAME_USED(i))
		i++;
	lowest_free_frame = i;

	return page;
}

uint get_free_pe()
{
	uint pe = lowest_free_pe;
	uint i = lowest_free_pe + 1;
	while (i < PDT_ENTRIES * PT_ENTRIES && PTE_USED(i))
		i++;
	lowest_free_pe = i;

	return pe;
}

void init_page_bitmap()
{
	// Clear bitmap
	memset(frame_bitmap, 0, PDT_ENTRIES * PT_ENTRIES / (sizeof(uint) * 8) * sizeof(uint));

	// Scan memory and set bitmap accordingly
	for (int i = 0; i < PT_ENTRIES; i++)
	{
		if (!(asm_pt1->entries[i] & PAGE_PRESENT))
			continue;

		uint page_id = (asm_pt1->entries[i] >> 12) & 0x3FF;
		frame_bitmap[i / 32] |= 1 << (page_id % 32);
	}

	// Display kernel size
	uint c = 0;
	for (; asm_pt1->entries[c] & PAGE_PRESENT; c++);
	printf_info("Kernel spans over %u pages", c);

	if (c == PT_ENTRIES)
		printf_error("Kernel needs more space than available in 1 page table");

	// Compute lowest_free_frame
	for (lowest_free_frame = 0; FRAME_USED(lowest_free_frame); lowest_free_frame++);
}

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
	{
		printf_error("Not enough space to initialize memory, kernel is too large");
		__asm__ volatile("hlt");
	}
	asm_pt1->entries[1022] = new_page_phys_addr | PAGE_WRITE | PAGE_PRESENT;
	//MARK_FRAME_USED(new_page_frame_id); No need, will be done in allocate_page

	// Apply changes
	INVALIDATE_PAGE(768, 1022);

	// Newly allocated page will be the first of the page tables. It will map itself and the 1023 other page tables.
	// Get pointer to newly allocated page, casting to page_table*.
	page_tables = (page_table_t*)VIRT_ADDR(768, 1022, 0);

	// Allocate pages 1024 to 2047, ie allocate space of all the page tables
	for (uint i = 0; i < PT_ENTRIES; ++i)
		allocate_page(PDT_ENTRIES + i, i);

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
	/** Clear unused page tables (all but 769)
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

	lowest_free_pe = 770 * PDT_ENTRIES; // Kernel is (for now at least) only allowed to allocate in higher half
}

void load_grub_modules(multiboot_info* multibootInfo)
{
	grub_modules = (GRUB_module*)malloc(multibootInfo->mods_count * sizeof(GRUB_module));

	for (uint i = 0; i < multibootInfo->mods_count; ++i)
	{
		multiboot_module_t* module = &((multiboot_module_t*)(multibootInfo->mods_addr + 0xC0000000))[i];

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

void init_mem(struct multiboot_info* multibootInfo)
{
	init_page_bitmap();
	allocate_page_tables();
	freep = &base;
	load_grub_modules(multibootInfo);
}

void* sbrk(uint n)
{
	// Memory full
	if (lowest_free_frame == PT_ENTRIES * PDT_ENTRIES)
		return nullptr;

	uint b = lowest_free_pe; /* block beginning page index */
	uint e; /* block end page index + 1*/
	unsigned num_pages_requested = n / PAGE_SIZE + (n % PAGE_SIZE == 0 ? 0 : 1);

	// Try to find contiguous free virtual block of memory
	while (1)
	{
		// Maximum possibly free memory is too small to fulfill request
		if (b + num_pages_requested > PDT_ENTRIES * PT_ENTRIES)
			return nullptr;

		uint rem = num_pages_requested;
		uint j = b;

		// Explore contiguous free blocks while explored block size does not fulfill the request
		while (!(PTE_USED(b)) && rem > 0)
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
	for (uint i = b; i < e; ++i)
		allocate_page(get_free_page(), i);

	lowest_free_pe = e;
	while (PTE_USED(lowest_free_pe))
		lowest_free_pe++;

	// Allocated memory block virtually starts at page b. Return it.
	return (void*)(((b / PDT_ENTRIES) << 22) | ((b % PDT_ENTRIES) << 12));
}

memory_header* more_kernel(uint n)
{
	if (n < N_ALLOC)
		n = N_ALLOC;

	memory_header* h = (memory_header*)sbrk(sizeof(memory_header) * n);

	if ((int*)h == nullptr)
		return nullptr;

	h->s.size = n;

	uint byte_size = sizeof(memory_header) * n;
	uint free_bytes_save = free_bytes;
	free_bytes = -byte_size; // Prevent free from freeing pages we just allocated
	free((void*)(h + 1));
	free_bytes = free_bytes_save + byte_size;

	return freep;
}

extern "C" void* malloc(uint n)
{
	memory_header* c;
	memory_header* p = freep;
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
			free_bytes -= nunits * sizeof(memory_header);
			freep = p;
			return (void*)(c + 1);
		}
		if (c == freep) /* wrapped around free list */
		{
			if ((c = more_kernel(nunits)) == nullptr)
				return nullptr; /* none left */
		}
	}
}

extern "C" void* calloc(size_t nmemb, size_t size)
{
	if (!nmemb || !size)
		return malloc(1);
	size_t total_size;
	if (__builtin_mul_overflow(nmemb, size, &total_size))
		return NULL;

	void* mem = malloc(total_size);
	if (!mem)
		return NULL;
	memset((char*)mem, 0, total_size);

	return mem;
}

extern "C" void free(void* ptr)
{
	memory_header* c = (memory_header*)ptr - 1;
	memory_header* p;
	free_bytes += c->s.size * sizeof(memory_header);

	// Loop until p < c < p->s.ptr
	for (p = freep; !(c > p && c < p->s.ptr); p = p->s.ptr)
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
	freep = p;

	// Free pages
	if (free_bytes >= FREE_THRESHOLD)
		free_release_pages();
}

void free_release_pages()
{
	memory_header* p = freep;
	for (memory_header* c = freep->s.ptr;; p = c, c = c->s.ptr)
	{
		// printf(" - %p", c);
		uint block_byte_size = c->s.size * sizeof(memory_header);
		uint aligned_addr_base =
			((uint)c + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); // Start addr of first page
		uint d = aligned_addr_base - (uint)c;
		uint aligned_free_bytes = d >= block_byte_size ? 0 : block_byte_size - d;
		aligned_free_bytes -= aligned_free_bytes & (PAGE_SIZE - 1); // -= aligned_free_bytes % PAGE_SIZE

		// Skip small block
		if (aligned_free_bytes < PAGE_SIZE)
		{
			// Arrived at the end of the list
			if (c == freep)
				break;
			continue;
		}

		// printf("\nFreeing from 0x%x to 0x%x\n", aligned_addr_base, aligned_addr_base + aligned_free_bytes);

		unsigned n_pages = aligned_free_bytes >> 12; // Num pages to free, aligned_free_bytes / PAGE_SIZE
		unsigned free_size = aligned_free_bytes / sizeof(memory_header); // Freed memory size in sizeof(memory_header)
		uint remaining_size = c->s.size - free_size;

		// Adjust memory headers
		// Page aligned block entirely removed (remaining_size can't be 0 if block isn't page aligned)
		if (remaining_size == 0)
		{
			p->s.ptr = c->s.ptr; // Link previous to next

			// Rollback c to previous mem header cause the mem region c is in will be deallocated,
			// thus the for loop would segfault without this instruction. With it, the for loop
			// will gently iterate to the next mem header as we just updated previous' next
			c = p;
		}
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

				// Link previous block to second block - previous block is p if first block does bot exist
				(first_block_size == 0 ? p : c)->s.ptr = c + first_block_size + free_size;

				// Setup second block
				c += first_block_size + free_size; // Move to second block location
				c->s.size = second_block_size; // Write size
				c->s.ptr = next; // Link with next
			}
		}

		// Free pages
		for (uint i = 0; i < n_pages; ++i)
		{
			uint aligned_addr = aligned_addr_base + (i << 12);
			uint pde = aligned_addr >> 22;
			uint pte = (aligned_addr >> 12) & 0x3FF;
			free_page(pde, pte);
		}

		free_bytes -= aligned_free_bytes; // Update free_bytes

		// Arrived at the end of the list
		if (c == freep + free_size)
		{
			freep += free_size;
			break;
		}
	}

	// printf("\n====\n");
}

void allocate_page(uint frame_id, uint page_id)
{
	// Write PTE
	PTE(page_id) = FRAME_ID_ADDR(frame_id) | PAGE_WRITE | PAGE_PRESENT;
	MARK_FRAME_USED(frame_id); // Internal allocation registration
}

void free_page(uint pde, uint pte)
{
	uint frame_id = page_tables[pde].entries[pte] >> 12;

	// Write PTE
	page_tables[pde].entries[pte] = 0;
	INVALIDATE_PAGE(pde, pte);
	MARK_FRAME_FREE(frame_id); // Internal deallocation registration

	if (frame_id < lowest_free_frame)
		lowest_free_frame = frame_id;
}

void allocate_page_user(uint frame_id, uint page_id)
{
	// Write PTE
	PTE(page_id) = FRAME_ID_ADDR(frame_id) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;
	MARK_FRAME_USED(frame_id); // Internal allocation registration
}

void* page_aligned_malloc(uint size)
{
	uint total_size = size + sizeof(void*) + PAGE_SIZE;
	void* base_addr = malloc(total_size);
	if (base_addr == nullptr)
		return nullptr;

	uint aligned_addr = ((uint)base_addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	((void**)aligned_addr)[-1] = base_addr;

	return (void*)aligned_addr;
}

void page_aligned_free(void* ptr)
{
	void* base_addr = ((void**)ptr)[-1];
	free(base_addr);
}

pdt_t* get_pdt()
{
	return pdt;
}

page_table_t* get_page_tables()
{
	return page_tables;
}

GRUB_module* get_grub_modules()
{
	return grub_modules;
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
