#ifndef INCLUDE_MEMORY_H
#define INCLUDE_MEMORY_H

#include "../boot/multiboot.h"
#include <kstddef.h>

#define PAGE_SIZE		4096
#define PAGE_PRESENT	0x1
#define PAGE_WRITE		0x2
#define PAGE_USER		0x4
#define PAGE_LAZY_ZERO	0x200

#define PDT_ENTRIES 1024
#define PT_ENTRIES 1024

/* Minimum memory block size to allocate to reduce calls to sbrk */
#define N_ALLOC 1024
#define FREE_THRESHOLD (10 * PAGE_SIZE) // Maximum bytes freed before attempting to free pages

#define STACK_SIZE 4096
#define KERNEL_VIRTUAL_BASE 0xC0000000

#define ADDR_PDE(addr) ((addr) >> 22)
#define ADDR_PTE(addr) (((addr) >> 12) & 0x3FF)
#define ADDR_PAGE(addr) ((addr) >> 12)
#define INVALIDATE_PAGE(pde, pte) __asm__ volatile("invlpg (%0)" : : "r" (VIRT_ADDR(pde, pte, 0)));
#define FRAME_ID_ADDR(i) ((i) << 12)
#define VIRT_ADDR(pde, pte, offset) ((pde) << 22 | (pte) << 12 | (offset))
#define PTE_PHYS_ADDR(i) (FRAME_ID_ADDR((PDT_ENTRIES + (i))))
#define PTE_USED(page_tables, i) (PTE(page_tables, i) & (PAGE_PRESENT | PAGE_LAZY_ZERO))
#define PTE(page_tables, i) (page_tables[(i) >> 10].entries[(i) & 0x3FF])
#define FRAME_USED(i) (Memory::frame_to_page[i] != (uint)-1)
#define FRAME_FREE(i) !(FRAME_USED(i))
#define MARK_FRAME_USED(frame_id, page_id) frame_to_page[frame_id] = page_id
#define MARK_FRAME_FREE(i) frame_to_page[i] = (uint)-1
#define PHYS_ADDR(page_tables, virt_addr) ((page_tables[(virt_addr) >> 22].entries[((virt_addr) >> 12) & 0x3FF] & ~0x3FF) | ((virt_addr) & 0xFFF))

//https://wiki.osdev.org/Paging

class Process;

namespace Memory
{
	typedef struct
	{
		uint entries[PT_ENTRIES];
	} page_table_t;

	typedef struct
	{
		uint entries[PDT_ENTRIES];
	} pdt_t;

	typedef double Align;

	typedef union memory_header /* memory block header */
	{
		struct
		{
			union memory_header* ptr; /* next block */
			uint size; /* size of this block */
		} s;

		Align x; /* force alignment of blocks */
	} memory_header;

	typedef struct
	{
		uint start_addr, size;
	} GRUB_module;

	extern "C" void boot_page_directory();

	extern "C" void boot_page_table1();

	extern pdt_t* pdt;
	extern page_table_t* asm_pt1;
	extern page_table_t* page_tables;
	extern GRUB_module* grub_modules;
	extern uint* frame_to_page;
	extern Process* kernel_process;

	/** Reload cr3 which will acknowledge every pte change and invalidate TLB */
	extern "C" void reload_cr3_asm();

	/** Initialize memory, by referencing free pages, allocating pages to store 1024 pages tables
	 *
	 * @param minfo Multiboot info structure
	 */
	void init(const multiboot_info_t* minfo);

	multiboot_info_t* register_multiboot_info(const multiboot_info_t* minfo);

	void* mmap(size_t length, [[maybe_unused]] int prot, char* path, [[maybe_unused]] uint offset);

	/**
	 * Allocate page-aligned memory
	 *
	 * @param size Size of memory to allocate
	 *
	 * @return Pointer to page-aligned memory
	 */
	void* malloca(uint size);

	void* calloca(size_t nmemb, size_t size);

	/**
	 * Allocates memory which is both virtually and physically contiguous.
	 * Does not go through the classical malloc process, thus the resulting pointer cannot be given to free.
	 * Memory acquired with this function has to be released by hand.
	 * @param n Size of memory block to allocate
	 * @return Pointer to beginning of memory block, nullptr on failure
	 */
	void* physically_aligned_malloc(uint n);

	/**
	 * Free page-aligned memory
	 *
	 * @param ptr Pointer to page-aligned memory to free
	 */
	void freea(void* ptr);

	/** Allocate a page
	 *
	 * @param frame_id Frame id
	 * @param page_id Page id
	 */
	template<bool lazy_zero>
	void allocate_page(uint frame_id, uint page_id);

	template<bool lazy_zero>
	void allocate_page(uint page_id);

	/** Allocate a page with user permissions
	 *
	 * @param frame_id Frame id
	 * @param page_id Page id
	 */
	template<bool lazy_zero>
	void allocate_page_user(uint frame_id, uint page_id);

	template<bool lazy_zero>
	void allocate_page_user(uint page_id);

	/**
	 * Free a page
	 * @param address address to free
	 * @param process process to get the relevant address space from to correctly interpret the address, nullptr
	 * if kernel
	 */
	void free_page(uint address, const Process* process);

	/** Get index of lowest free page id and update lowest_free_page to next free page id */
	uint get_free_frame();

	/** Get index of lowest free page entry id in higher half and update lowest_free_pe to next free page id */
	uint get_free_pe();

	/** Get index of lowest free page entry id in lower half and update lowest_free_pe_user to next free page id */
	uint get_free_pe_user();

	/** Get pointer to top of kernel stack */
	uint* get_stack_top_ptr();

	void* mmap(int prot, const char* path, uint offset = 0, size_t length = 0);

	/** Attempts to identity map a memory region. Returns success status **/
	bool identity_map(uint addr, uint size);

	/**
	 * Tries to handle a page fault. Checks whether the fault was caused by lazy page allocation. If so,
	 * actually allocates the page and returns true. Otherwise, returns false.
	 * @param current_process process that was running when the page fault occurred
	 * @param fault_address address which caused the fault
	 * @return whether the fault has been handled or is an error
	 */
	bool page_fault_handler(Process* current_process, uint fault_address);

	/**
	 * Attempts to map some memory that has been written somewhere by some entity (BIOS, GRUB...) with a known physical
	 * address into page tables, thus making the data accessible with paging enabled.
	 * @param physical_address start physical address of memory block to register
	 * @param size size of the block to register
	 * @return virtual address of the mapped memory, nullptr on failure
	 */
	void* register_physical_data(uint physical_address, uint size);

	/**
	 *
	 * @param n number of pages requested
	 * @param process process owning the relevant address space
	 * @return the first page entry id of a contiguous block of n pages, or -1 if no such block exists
	 */
	uint get_contiguous_pages(uint n, const Process* process);
}

class Process;

/** Tries to allocate a contiguous block of memory
 *
 * @param n Size of the block in bytes
 * @return Address of the beginning of allocated block if allocation was successful, NULL otherwise
 */
extern "C" void* malloc(uint n);

extern "C" void* realloc(void* ptr, size_t size);

/** Tries to allocate a contiguous block of memory on pages marked with PAGE_USER
 *
 * @param n Size of the block in bytes
 * @param process process to get the relevant address space from to correctly interpret the address, nullptr
 * if kernel
 * @return Address of the beginning of allocated block if allocation was successful, NULL otherwise
 */
template <bool lazy_zero>
void* malloc(uint n, Process* process);

void* calloc(size_t nmemb, size_t size, Process* process);

extern "C" void* calloc(size_t nmemb, size_t size);

/** Frees some memory
 *
 * @param ptr Pointer to the memory block to free
 */
extern "C" void free(void* ptr);

/** Frees some process memory
 *
 * @param ptr Pointer to the memory block to free
 * @param process process to get the relevant address space from to correctly interpret the address, nullptr
 * if kernel
 */
void free(void* ptr, Process* process);

void* realloc(void* ptr, size_t size, Process* process);

#endif //INCLUDE_MEMORY_H
