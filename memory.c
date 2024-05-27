#include "memory.h"
#include "fb.h"

#define PAGE_USED(i) page_bitmap[i / 32] & (1 << (i % 32))
#define PAGE_FREE(i) !(PAGE_USED(i))
#define MARK_PAGE_AS_ALLOCATED(i) page_bitmap[i / 32] |= 1 << (i % 32)

extern void boot_page_directory();

unsigned int* pdt = (unsigned int*) boot_page_directory;

extern void boot_page_table1();

page_table_t* pt1 = (page_table_t*) boot_page_table1;

unsigned int page_bitmap[32];
unsigned int lowest_free_page_entry;
unsigned int lowest_free_page;

mem_header base = {.s = {&base, 0}};
mem_header* freep; /* Lowest free block */

unsigned int alloc_page(page_table_t* pt)
{
	unsigned int i = lowest_free_page_entry;
	if (i == PT_ENTRIES)
	{
		fb_error();
		fb_write("Page table full\n");
	}

	unsigned int physical_address = (unsigned int) ((void*) (pt->entries[i - 1] >> 12)) + 1;
	pt->entries[i] = (physical_address) << 12 | PAGE_PRESENT | PAGE_WRITE/* | PAGE_USER*/;

	do
	{ lowest_free_page_entry++; }
	while (pt->entries[lowest_free_page_entry] & PAGE_PRESENT);

	if (lowest_free_page_entry == PT_ENTRIES)
		fb_write_error_msg("pt1 is full\n");

	return i;
}

void* allocate_page_frame()
{
	unsigned int i = alloc_page(pt1);
	return (void*) ((768 << 22) | i << 12);
}

void init_mem()
{
	unsigned int i = 0;
	for (int j = 0; j < 32; ++j)
		page_bitmap[j] = 0;

	while (i < PT_ENTRIES && pt1->entries[i] & PAGE_PRESENT)
	{
		page_bitmap[i / 32] |= 1 << (i % 32);
		i++;
	}

	if (i == PT_ENTRIES)
		fb_write_error_msg("Kernel needs more space than available in 1 page table\n");

	lowest_free_page_entry = lowest_free_page = i;

	freep = &base;
}

void* sbrk(unsigned int n)
{
	// pt1 is full
	if (lowest_free_page_entry == PT_ENTRIES)
		return 0x00;

	unsigned int b = lowest_free_page; /* block beginning page index */
	unsigned int e = b; /* block end page index + 1*/
	unsigned int rem = n + PAGE_SIZE; /* how many bytes remain to be allocated + PAGE_SIZE */

	while (rem > PAGE_SIZE)
	{
		rem = n + PAGE_SIZE;

		// Browse free pages
		unsigned int j = b;
		while (rem > PAGE_SIZE && PAGE_FREE(j))
		{
			rem -= PAGE_SIZE;
			j++;
		}

		// Block is big enough
		if (rem <= PAGE_SIZE)
		{
			e = j; /* Block spans from page b to e - 1 */
			break;
		}

		b++; /* Increment start page index */
	}

	unsigned int page_entry_id = lowest_free_page_entry - 1;
	for (unsigned int j = b; j < e; ++j)
	{
		// Find free page entry
		do
		{ page_entry_id++; }
		while (page_entry_id < PT_ENTRIES && pt1->entries[page_entry_id] & PAGE_PRESENT);

		if (page_entry_id == PT_ENTRIES)
			fb_write_error_msg("PT 768 full\n");

		// Map page
		pt1->entries[page_entry_id] = j << 12 | PAGE_PRESENT | PAGE_WRITE;
		MARK_PAGE_AS_ALLOCATED(j);
	}

	// Update lowest_free_page_entry
	while (lowest_free_page_entry < PT_ENTRIES && pt1->entries[lowest_free_page_entry] & PAGE_PRESENT)
		lowest_free_page_entry++;
	// Update lowest_free_page
	while (PAGE_USED(lowest_free_page))
		lowest_free_page++;

	return (void*) (768 << 22 | b << 12);
}

mem_header* more_kernel(unsigned int n)
{
	if (n < N_ALLOC)
		n = N_ALLOC;

	mem_header* h = sbrk(sizeof(mem_header) * n);

	if ((int*) h == 0x00)
		return 0x00;

	h->s.size = n;

	free((void*) (h + 1));

	return freep;
}

/* malloc: general-purpose storage allocator */
void* malloc(unsigned int n)
{
	mem_header* c;
	mem_header* p = freep;
	unsigned nunits = (n + sizeof(mem_header) - 1) / sizeof(mem_header) + 1;

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
	mem_header* c = (mem_header*) ptr - 1;
	mem_header* p;

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
