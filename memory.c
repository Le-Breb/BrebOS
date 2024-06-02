#include "memory.h"
#include "fb.h"
#include "gdt.h"

extern void boot_page_directory();

extern void boot_page_table1();

unsigned int* asm_pdt = (unsigned int*) boot_page_directory;
pdt_t* pdt = (pdt_t*) boot_page_directory;
page_table_t* asm_pt1 = (page_table_t*) boot_page_table1;
page_table_t* page_tables;

unsigned int page_bitmap[32 * 1024];
unsigned int lowest_free_page_entry;
unsigned int lowest_free_page;

memory_header base = {.s = {&base, 0}};
memory_header* freep; /* Lowest free block */

unsigned int loaded_grub_modules = 0;
GRUB_module grubModules[10];

/** Tries to allocate a contiguous block of memory
 * @param n Size of the block in bytes
 *  @return A pointer to a contiguous block of n bytes or NULL if memory is full
 */
void* sbrk(unsigned int n);

void set_pdt_entry(pdt_t* pdt, unsigned int num, unsigned int base, char access);

void set_pt_entry(page_table_t* pt, unsigned int num, unsigned int base, char access);

unsigned int get_free_page_id();

void init_page_bitmap();

/** Tries to allocate a contiguous block of (virtual) memory
 *
 * @param n How many bytes to allocate
 * @return Pointer to allocated memory if allocation was successful, NULL otherwise
 */
memory_header* more_kernel(unsigned int n);

extern void reload_cr3();

/** Allocate 1024 pages to store the 1024 page tables required to map all the memory. \n
 * 	The page table that maps kernel pages is moved into the newly allocated array of page tables and then freed. \n
 * 	This function also set page_tables to point to the array of newly allocated page tables
 * 	@details
 *  Page tables will be written in physical pages 0 to 1023. \n
 *	Page tables are virtually located on pages 1024*769 to 1024*769+1023. \n
 *	@note Any call to malloc will never return 0 on a successful allocation, as virtual address 0 maps the first page
 *	table, and will consequently never be returned by malloc if allocation is successful. \n
 *	NULL will then always be considered as an invalid address.
 *	@warning This function shall only be called once
 */
void allocate_page_tables();

/**
 * Find new lowest page entry.
 * This function is typically called after allocating a page at lowest_page_entry th entry
 */
void update_lowest_page_entry();

process* allocate_process_memory(unsigned int code_size);

/** Mark the pages where GRUB module is loaded as allocated
 *
 * @param multibootInfo GRUB multiboot struct pointer
 */
void load_grub_modules(struct multiboot_info* multibootInfo);

unsigned int get_free_page_id()
{
	unsigned int page_id = lowest_free_page;
	unsigned int i = lowest_free_page + 1;
	while (i < PDT_ENTRIES * PT_ENTRIES && PAGE_USED(i))
		i++;
	lowest_free_page = i;

	return page_id;
}

void init_page_bitmap()
{
	for (int i = 0; i < 32 * 1024; ++i)
		page_bitmap[i] = 0;

	for (int i = 0; i < PT_ENTRIES; i++)
	{
		if (!(asm_pt1->entries[i] & PAGE_PRESENT))
			continue;

		unsigned int page_id = asm_pt1->entries[i] >> 12;
		page_bitmap[i / 32] |= 1 << (page_id % 32);
		i++;
	}

	lowest_free_page = 0;
	for (; asm_pt1->entries[lowest_free_page] & PAGE_PRESENT; lowest_free_page++);
	fb_write("Kernel spans over ");
	fb_write_dec(lowest_free_page);
	fb_write(" pages\n");

	if (lowest_free_page == PT_ENTRIES)
		fb_write_error_msg("Kernel needs more space than available in 1 page table\n");
}

void allocate_page_tables()
{
	// Save for later use
	unsigned int asm_pt1_page_table_entry = ((unsigned int) asm_pt1 >> 12) & 0x3FF;

	// Allocate page 1024 + 769
	unsigned int new_page_phys_id = PDT_ENTRIES + 769;
	unsigned int new_page_phys_addr = PAGE_ID_PHYS_ADDR(new_page_phys_id);

	// Map it in entry 1022 of asm pt
	asm_pt1->entries[1022] = new_page_phys_addr | PAGE_WRITE | PAGE_PRESENT;
	MARK_PAGE_AS_ALLOCATED(new_page_phys_id);

	// Apply changes
	reload_cr3();

	// Newly allocated page will be the first of the page tables. It will map itself and the 1023 other page tables.
	// Get pointer to newly allocated page, casting to page_table*.
	page_tables = (page_table_t*) VIRTUAL_ADDRESS(768, 1022, 0);

	// Allocate pages 1024 to 2047, ie allocate space of all the page tables
	for (int i = 0; i < PT_ENTRIES; ++i)
	{
		unsigned int page_phys_id = PDT_ENTRIES + i;
		unsigned int page_phys_addr = PAGE_ID_PHYS_ADDR(page_phys_id);

		page_tables[0].entries[i] = page_phys_addr | PAGE_WRITE | PAGE_PRESENT;
		MARK_PAGE_AS_ALLOCATED(page_phys_id);
	}

	// Indicate that newly allocated page is a page table. Map it in pdt[769]
	pdt->entries[769] = asm_pt1->entries[1022];
	asm_pt1->entries[1022] = 0; // Not needed anymore as the page it maps now maps itself as it became a page table

	reload_cr3(); // Apply changes

	// Update pointer using pdt[769]
	page_tables = (page_table_t*) VIRTUAL_ADDRESS(769, 0, 0);

	// Clear unused pages
	for (int i = 0; i < PDT_ENTRIES; ++i) // Skip first page as it maps all the page tables' pages
	{
		pdt->entries[i] |= PAGE_USER; // Let user access all page tables. Access will be defined in page table entries
		if (i == 769)
			continue;
		for (int j = 0; j < PT_ENTRIES; ++j)
			page_tables[i].entries[j] = 0;
	}

	// Copy asm_pt1 (the page table that maps the pages used by the kernel) in appropriated page table
	for (int i = 0; i < PT_ENTRIES; ++i)
		page_tables[768].entries[i] = asm_pt1->entries[i];

	for (int i = 0; i < 768; ++i)
		pdt->entries[i] = PAGE_ENTRY_ID_PHYS_ADDR(i) | PAGE_PRESENT | PAGE_WRITE;

	// Update pdt[entry] to point to new location of kernel page table
	pdt->entries[768] = PAGE_ID_PHYS_ADDR((PDT_ENTRIES + 768)) | (pdt->entries[768] & 0x3FF);

	for (int i = 770; i < PDT_ENTRIES; ++i)
		pdt->entries[i] = PAGE_ENTRY_ID_PHYS_ADDR(i) | PAGE_PRESENT | PAGE_WRITE;

	// Deallocate asm_pt1's page
	page_tables[768].entries[asm_pt1_page_table_entry] = 0;

	reload_cr3(); // Apply changes

	lowest_free_page_entry = 770 * PDT_ENTRIES; // Kernel is (for now at least) only allowed to allocate in high half
}

void load_grub_modules(struct multiboot_info* multibootInfo)
{
	for (unsigned int i = 0; i < multibootInfo->mods_count; ++i)
	{
		multiboot_module_t* module = &((multiboot_module_t*) (multibootInfo->mods_addr + 0xC0000000))[i];

		// Set module page as present
		unsigned int module_phys_start_page_id = (unsigned int) module->mod_start / PAGE_SIZE;
		unsigned int mod_size = module->mod_end - module->mod_start;
		unsigned int required_pages = mod_size / PAGE_SIZE + (mod_size % PAGE_SIZE == 0 ? 0 : 1);

		//unsigned int start_page_entry_id = lowest_free_page_entry;
		//unsigned int start_page_pdt_id = start_page_entry_id / PDT_ENTRIES;
		//unsigned int start_page_pt_id = start_page_entry_id % PDT_ENTRIES;
		//unsigned int start_virtual_address = start_page_pdt_id << 22 | start_page_pt_id << 12;

		unsigned int page_entry_id = lowest_free_page_entry;

		grubModules[i].start_addr = ((page_entry_id / PDT_ENTRIES) << 22) | ((page_entry_id % PDT_ENTRIES)<< 12);
		grubModules[i].size = mod_size;

		// Mark module's code pages as allocated
		for (unsigned int j = 0; j < required_pages; ++j)
		{
			unsigned int phys_page_id = module_phys_start_page_id + j;
			allocate_page_user(phys_page_id, page_entry_id);

			page_entry_id++;
		}

		lowest_free_page_entry = page_entry_id;

		//lowest_free_page_entry = page_entry_id;
		//typedef void (* call_module_t)(void);
		//call_module_t start_program = (call_module_t) start_virtual_address;
		//start_program();
	}

	loaded_grub_modules = multibootInfo->mods_count;
}

void init_mem(struct multiboot_info* multibootInfo)
{
	init_page_bitmap();
	allocate_page_tables();
	load_grub_modules(multibootInfo);

	freep = &base;
}

void* sbrk(unsigned int n)
{
	// Memory full
	if (lowest_free_page == PT_ENTRIES * PDT_ENTRIES)
		return 0x00;

	unsigned int b = lowest_free_page_entry; /* block beginning page index */
	unsigned int e; /* block end page index + 1*/
	unsigned num_pages_requested = n / PAGE_SIZE + (n % PAGE_SIZE == 0 ? 0 : 1);;

	// Try to find contiguous free virtual block of memory
	while (1)
	{
		// Maximum possibly free memory is too small to fulfill request
		if (b + num_pages_requested > PDT_ENTRIES * PT_ENTRIES)
			return 0x00;

		unsigned int rem = num_pages_requested;
		unsigned int page_entry_pdt_id = b / PDT_ENTRIES;
		unsigned int page_entry_pt_id = b % PDT_ENTRIES;
		unsigned int j = b;

		// Explore contiguous free blocks while explored block size does not fulfill the request
		while (!(page_tables[page_entry_pdt_id].entries[page_entry_pt_id] & PAGE_PRESENT) && rem > 0)
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
	for (unsigned int i = b; i < e; ++i)
		allocate_page(get_free_page_id(), i);

	lowest_free_page_entry = e;
	while (page_tables[lowest_free_page_entry / PDT_ENTRIES].entries[lowest_free_page_entry % PDT_ENTRIES] &
		   PAGE_PRESENT)
		lowest_free_page_entry++;

	// Allocated memory block virtually starts at page b. Return it.
	return (void*) (((b / PDT_ENTRIES) << 22) | ((b % PDT_ENTRIES) << 12));
}

memory_header* more_kernel(unsigned int n)
{
	if (n < N_ALLOC)
		n = N_ALLOC;

	memory_header* h = sbrk(sizeof(memory_header) * n);

	if ((int*) h == 0x00)
		return 0x00;

	h->s.size = n;

	free((void*) (h + 1));

	return freep;
}

void* malloc(unsigned int n)
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
			freep = p;
			return (void*) (c + 1);
		}
		if (c == freep) /* wrapped around free list */
		{
			if ((c = more_kernel(nunits)) == 0x00)
				return 0x00; /* none left */
		}
	}
}

void free(void* ptr)
{
	memory_header* c = (memory_header*) ptr - 1;
	memory_header* p;

	// Loop until p < c < p->s.ptr
	for (p = freep; !(c > p && c < p->s.ptr); p = p->s.ptr)
		//Break when arrived at the end of the list and c goes before beginning or after end

		if (p >= p->s.ptr && (c < p->s.ptr || c > p))
			break;

	// Join with upper neighbor
	if (c + c->s.size == p->s.ptr)
	{
		c->s.size += p->s.ptr->s.size;
		c->s.ptr = p->s.ptr->s.ptr;
	}
	else
		c->s.ptr = p->s.ptr;

	// Join with lower neighbor
	if (p + p->s.size == c)
	{
		p->s.size += c->s.size;
		p->s.ptr = c->s.ptr;
	}
	else
		p->s.ptr = c;

	// Set new freep
	freep = p;
}

void allocate_page(unsigned int phys_page_id, unsigned int page_id)
{
	unsigned int pdt_entry = page_id / PDT_ENTRIES;
	unsigned int pt_entry = page_id % PDT_ENTRIES;
    page_tables[pdt_entry].entries[pt_entry] = PAGE_ID_PHYS_ADDR(phys_page_id) | PAGE_WRITE | PAGE_PRESENT; \
    MARK_PAGE_AS_ALLOCATED(phys_page_id);
}

void allocate_page_user(unsigned int phys_page_id, unsigned int page_id)
{
	unsigned int pdt_entry = page_id / PDT_ENTRIES;
	unsigned int pt_entry = page_id % PDT_ENTRIES;
    page_tables[pdt_entry].entries[pt_entry] = PAGE_ID_PHYS_ADDR(phys_page_id) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT; \
    MARK_PAGE_AS_ALLOCATED(phys_page_id);
}

void run_module([[maybe_unused]] unsigned int module)
{
	// Allocate process memory
	unsigned int proc_size = sizeof (process);
	unsigned int req_pages = proc_size / PAGE_SIZE + (proc_size % PAGE_SIZE ? 1 : 0);

	process * proc = (process*)(((lowest_free_page_entry / PDT_ENTRIES) << 22) | ((lowest_free_page_entry % PDT_ENTRIES) << 12));

	// Allocate memory to store process struct
	for (unsigned int i = 0; i < req_pages; i++)
	{
		allocate_page(get_free_page_id(), lowest_free_page_entry);
		update_lowest_page_entry();
	}

	// Set process pdt entries
	for (int i = 0; i < PDT_ENTRIES; ++i)
	{
		unsigned int pt_virt_addr = (unsigned int)&proc->page_tables[i];
		unsigned int pt_phys_addr = PHYS_ADDR(pt_virt_addr);
		proc->pdt.entries[i] = pt_phys_addr | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;
	}

	// Copy kernel page tables into process' ones
	for (int i = 0; i < PDT_ENTRIES; ++i)
		for (int j = 0; j < PT_ENTRIES; ++j)
			proc->page_tables[i].entries[j] = page_tables[i].entries[j];

	unsigned int mod_size = grubModules[module].size;
	unsigned int mod_code_pages = mod_size / PAGE_SIZE + ((mod_size % PAGE_SIZE) ? 1 : 0); // Num pages code spans over

	// Map process code at 0x00
	for (unsigned int i = 0; i < mod_code_pages; ++i)
	{
		unsigned int page_pdt_id = i / PDT_ENTRIES;
		unsigned int page_pt_id = i % PDT_ENTRIES;

		unsigned int process_page_id = grubModules[module].start_addr / PAGE_SIZE + i;
		unsigned int process_page_pdt_id = process_page_id / PDT_ENTRIES;
		unsigned int process_page_pt_id = process_page_id % PDT_ENTRIES;

		// Swap process pages entries with first page entries to map process at 0x00
		if (proc->page_tables[page_pdt_id].entries[page_pt_id] != 0)
			fb_write_error_msg("Non empty pt entry\n");
		proc->page_tables[page_pdt_id].entries[page_pt_id] = proc->page_tables[process_page_pdt_id].entries[process_page_pt_id];
		proc->page_tables[process_page_pdt_id].entries[process_page_pt_id] = 0;
	}

	// Allocate process stack page. Register it in kernel and process page tables
	unsigned int p_stack_pdt_id = 767;
	unsigned int p_stack_pt_id = 1023;
	if (proc->page_tables[p_stack_pdt_id].entries[p_stack_pt_id] != 0)
		fb_write_error_msg("Stack page no empty\n");
	unsigned int stack_page_id = get_free_page_id();
	proc->page_tables[p_stack_pdt_id].entries[p_stack_pt_id] = PAGE_ENTRY(lowest_free_page_entry) = PAGE_ID_PHYS_ADDR(stack_page_id) | PAGE_USER | PAGE_PRESENT | PAGE_WRITE;
	update_lowest_page_entry();

	// Allocate trap handler stack page. Register it in kernel and process page tables
	unsigned int k_stack_pdt_id = 766;
	unsigned int k_stack_pt_id = 1023;
	if (proc->page_tables[k_stack_pdt_id].entries[k_stack_pt_id] != 0)
		fb_write_error_msg("Kernel stack page not empty\n");
	proc->page_tables[k_stack_pdt_id].entries[k_stack_pt_id] = PAGE_ENTRY(lowest_free_page_entry) = PAGE_ID_PHYS_ADDR(stack_page_id) | PAGE_PRESENT | PAGE_WRITE;
	update_lowest_page_entry();

	// Process pdt physical address, after process page tables
	unsigned int proc_pdt_phys_addr = page_tables[(unsigned int)&proc->pdt >> 22].entries[((unsigned int)&proc->pdt >> 12) & 0x3FF] & ~0x3FF;
	unsigned int p_stack_top = (p_stack_pdt_id << 22 | p_stack_pt_id << 12) + (PAGE_SIZE - 4);
	unsigned int k_stack_top = (k_stack_pdt_id << 22 | k_stack_pt_id << 12) + (PAGE_SIZE - 4);

	// Set TSS esp0 to point to the trap handler stack
	set_tss_kernel_stack(k_stack_top);

	// Jump
	unsigned int user_code_seg_sel = 0x18 | 0x03; // 3rd entry, pl 3
	unsigned int user_data_seg_sel = 0x20 | 0x03; // 4th entry, pl3
	unsigned int eflags = 0;
	unsigned int eip = 0; // 0 because process is mapped at 0x00
	user_mode_jump(proc_pdt_phys_addr, eip, user_code_seg_sel, eflags, p_stack_top, user_data_seg_sel);
}


void update_lowest_page_entry()
{
	do
	{
		lowest_free_page_entry++;
	}
	while (lowest_free_page_entry < PDT_ENTRIES * PT_ENTRIES && PAGE_ENTRY_USED(lowest_free_page_entry));
}