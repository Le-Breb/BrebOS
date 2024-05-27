#ifndef INCLUDE_MEMORY_H
#define INCLUDE_MEMORY_H
#define PAGE_SIZE 4096
#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4

#define PT_ENTRIES 1024

/* Minimum memory block size to allocate to reduce calls to sbrk */
#define N_ALLOC 1024

typedef struct
{
	unsigned int entries[PT_ENTRIES];
} page_table_t;

typedef double Align;

typedef union memory_header mem_header;
union memory_header
{ /* block header */
	struct
	{
		mem_header* ptr; /* next block if on free list */
		unsigned size; /* size of this block */
	} s;
	Align x; /* force alignment of blocks */
};

void* allocate_page_frame();

/** Tries to allocate a contiguous block of memory
 * @param n Size of the block in bytes
 *  @return A pointer to a contiguous block of n bytes or NULL if memory is full
 */
void* sbrk(unsigned int n);

void init_mem();

void* malloc(unsigned int n);

void free(void* ptr);

#endif //INCLUDE_MEMORY_H
