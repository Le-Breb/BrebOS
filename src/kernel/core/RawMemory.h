#pragma once

#include <cstdint>
#include "kstring.h"
#include "kstddef.h"
#include "MemoryDefines.h"

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

	typedef struct
	{
		uint start_addr, size;
	} GRUB_module;

	struct alloc_params
	{
		int flags;
		void* hint;
		bool mandatory_hint;
	};

	extern "C" void boot_page_directory();

	extern "C" void boot_page_table1();

	extern pdt_t* pdt;
	extern page_table_t* asm_pt1;
	extern page_table_t* page_tables;
	extern GRUB_module* grub_modules;
	extern uint* frame_to_page;
	extern uint* frame_rc;
	extern uint lowest_free_pe_user;
	extern uint lowest_free_frame;
	extern uint* stack_top_ptr;
	extern uint* lowest_free_pe; // Pointer to kernel_process.lowest_free_pe

	struct hint_info
	{
		uintptr_t hint;
		bool is_mandatory;
	};

	struct page_info
	{
		int prot, flags, policy;
		bool operator==(const page_info& other) const {return prot == other.prot && flags == other.flags && policy == other.policy;}
	};

	struct allocation
	{
		uintptr_t start, end;
		struct page_info page_info;
		bool used = false;

		[[nodiscard]]
		bool is_similar_and_contiguous(const allocation& other) const
		{
			return (start == other.end || end == other.start) && page_info == other.page_info && used == other.used;
		}

		bool operator==(const allocation& other) const {return !memcmp(this, &other, sizeof(*this));}
	};

	extern const page_info DEFAULT_K_PAGE_INFO;
	extern const page_info DEFAULT_U_PAGE_INFO;
	extern const hint_info DEFAULT_HINT_INFO;
	constexpr inline hint_info NULL_HINT{0, false};

	void allocate_page(uint frame_id, uint page_id, int policy);

	void allocate_page(uint page_id, int policy);

	/** Get index of lowest free page id and update lowest_free_page to next free page id */
	uint get_free_frame();

	/** Get index of lowest free page entry id in higher half and update lowest_free_pe to next free page id */
	uint get_free_pe();

	void free_page(uint page_id);

	uint get_contiguous_pages(uint n, const hint_info& hint_info, const page_table_t* pt, uint lowest_free_page_entry);
}
