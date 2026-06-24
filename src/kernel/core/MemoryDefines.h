#pragma once

#define PAGE_SIZE		4096
#define PAGE_PRESENT	0x1
#define PAGE_WRITE		0x2
#define PAGE_USER		0x4
#define PAGE_LAZY_ZERO	0x200 // Page is lazily zeroed, meaning it will be allocated and zeroed on first access
#define PAGE_COW		0x100 // Page is Copy On Write, meaning it is shared between processes and will be copied on first write
#define PAGE_SHRO		0x400 // Page is shared read-only, meaning it is shared between processes but not writable

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
#define MARK_FRAME_USED(frame_id, page_id) {Memory::frame_to_page[(frame_id)] = (page_id); \
	if (Memory::frame_rc[(frame_id)]) { irrecoverable_error("Trying to allocate frame which is already allocated"); } \
	Memory::frame_rc[(frame_id)]++;}
#define MARK_FRAME_FREE(i) {Memory::frame_to_page[(i)] = (uint)-1; if (Memory::frame_rc[(i)] > 1){irrecoverable_error \
	("Kernel is trying to free a frame which is referenced by a process");} \
else \
{\
Memory::frame_rc[(i)] = 0;}\
Memory::lowest_free_frame = min(Memory::lowest_free_frame, (i)); \
}
#define PHYS_ADDR(page_tables, virt_addr) ((page_tables[(virt_addr) >> 22].entries[((virt_addr) >> 12) & 0x3FF] & ~0x3FF) | ((virt_addr) & 0xFFF))

#define DEFAULT_K_PROT (PROT_READ | PROT_WRITE)
#define DEFAULT_K_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)
#define DEFAULT_K_POLICY (PAGE_WRITE | PAGE_PRESENT)
#define DEFAULT_U_PROT (PROT_READ | PROT_WRITE)
#define DEFAULT_U_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)
#define DEFAULT_U_POLICY (PAGE_WRITE | PAGE_PRESENT | PAGE_USER)

namespace Memory
{

}
