#ifndef INCLUDE_MEMORY_H
#define INCLUDE_MEMORY_H

#include "../boot/multiboot.h"
#include <kstddef.h>
#include <stddef.h>

#include "kstring.h"
#include "bits/off_t.h"
#include "RawMemory.h"
#include "Memtree.h"

//https://wiki.osdev.org/Paging

class Process;

namespace Memory
{
	extern Process* kernel_process;

	static inline constexpr uint FB_MODE_INFO_ADDR_WHEN_CUSTOM_BOOTLOADER_USED = 0x700;

	/** Reload cr3 which will acknowledge every pte change and invalidate TLB */
	extern "C" void reload_cr3_asm();

	/** Initialize memory, by referencing free pages, allocating pages to store 1024 pages tables
	 *
	 * @param minfo Multiboot info structure
	 */
	void init(const multiboot_info_t* minfo);

	multiboot_info_t* register_multiboot_info(const multiboot_info_t* minfo);

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

	/**
	 * Free a page
	 * @param address address to free
	 * @param process process to get the relevant address space from to correctly interpret the address, nullptr
	 * if kernel
	 */
	void free_page(uint address, const Process* process);

	/** Get index of lowest free page entry id in lower half and update lowest_free_pe_user to next free page id */
	uint get_free_pe_user();

	/** Get pointer to top of kernel stack */
	uint* get_stack_top_ptr();

	void* mmap(void* hint, size_t size, int prot, int flags, int fd, off_t offset, int& err, Process* process, bool lazy_zero, bool
	           page_user);

	/** Attempts to identity map a memory region. Returns success status **/
	bool identity_map(uint addr, uint size);

	/**
	 * Tries to handle a page fault. Checks whether the fault was caused by lazy page allocation. If so,
	 * actually allocates the page and returns true. Otherwise, returns false.
	 * @param current_process process that was running when the page fault occurred
	 * @param fault_address address which caused the fault
	 * @param write_access whether the fault was caused by a write access
	 * @return whether the fault has been handled or is an error
	 */
	bool page_fault_handler(Process* current_process, uint fault_address, bool write_access);

	/**
	 * Attempts to map some memory that has been written somewhere by some entity (BIOS, GRUB...) with a known physical
	 * address into page tables, thus making the data accessible with paging enabled.
	 * @param physical_address start physical address of memory block to register
	 * @param size size of the block to register
	 * @return virtual address of the mapped memory, nullptr on failure
	 */
	void* register_physical_data(uint physical_address, uint size);

	/**
	 * Translates a physical address to a virtual address
	 * @param phys_addr physical address to translate
	 * @return the corresponding virtual address if the provided physical address is mapped, (uint)-1 otherwise
	 */
	uint phys_to_virt_addr(uint phys_addr);

	int mprotect(void* addr, size_t len, int prot, const Process* process);

	/** Tries to allocate a contiguous block of memory
	 * @param num_pages_requested Size of the block in bytes
	 * @param page_info page information
	 * @param hint_info hint information
	 * @param process process to get the relevant address space from to correctly interpret the address, nullptr
	 * if kernel
	 *  @return A pointer to a contiguous block of n bytes or NULL if memory is full
	 */
	void* sbrk(uint num_pages_requested, const page_info& page_info, const hint_info& hint_info, Process* process);
}

class Process;

/** Tries to allocate a contiguous block of memory
 *
 * @param n Size of the block in bytes
 * @return Address of the beginning of allocated block if allocation was successful, NULL otherwise
 */
extern "C" void* malloc(uint n);

extern "C" void* realloc(void* ptr, size_t size);

void* lazy_malloc(uint n);

/** Tries to allocate a contiguous block of memory on pages marked with PAGE_USER
 *
 * @param n Size of the block in bytes
 * @param page_info page_info
 * @param process process to get the relevant address space from to correctly interpret the address, nullptr
 * if kernel
 * @return Address of the beginning of allocated block if allocation was successful, NULL otherwise
 */
void* malloc(uint n, const Memory::page_info& page_info, Process* process);

void* calloc(size_t nmemb, size_t size, const Memory::page_info& page_info, Process* process);

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
Memory::MemTree::FreeState free(void* ptr, Process* process);

Memory::MemTree::ReallocState realloc(void* ptr, size_t size, Process* process, void*& new_address);

#endif //INCLUDE_MEMORY_H
