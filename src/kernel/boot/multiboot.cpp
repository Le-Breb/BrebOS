#include "multiboot.h"

#include "../core/fb.h"
#include "../core/memory.h"

const multiboot_info_t* Multiboot::multiboot_info = nullptr;

void Multiboot::init(const multiboot_info_t* multiboot_info)
{
	Multiboot::multiboot_info = (multiboot_info_t*)multiboot_info;
}

void* Multiboot::get_tag(uint32_t type)
{
	auto* ptr = (uint8_t*)multiboot_info->tags; // skip total_size and reserved
	while ((uint32_t)(ptr - (uint8_t*)multiboot_info) < multiboot_info->total_size) {
		auto* tag = (struct multiboot_tag*)ptr;
		if (tag->type == 0)
			return nullptr;

		if (tag->type == type)
			return tag;

		ptr += (tag->size + 7) & ~7; // align to 8 bytes
	}

	return nullptr;
}

// ===================================== MULTIBOOT1=================================

/*void Multiboot::print_mmap(uint ebx)
{
	multiboot_info* mboot_header = (multiboot_info*) (ebx + KERNEL_VIRTUAL_BASE);
	uint memory_slots = mboot_header->mmap_length / sizeof(multiboot_memory_map_t);

	uint mem_map_start = (uint) mboot_header->mmap_addr + KERNEL_VIRTUAL_BASE;
	uint mem_map_size = (uint) mboot_header->mmap_length;
	//uint mem_map_end = (uint) mboot_header->mmap_addr + (uint) mboot_header->mmap_length;

	multiboot_memory_map_t* mmap_entries = new multiboot_memory_map_t[memory_slots];
	memcpy((char*) mmap_entries, (char*) mem_map_start, mem_map_size);

	printf("Memory map:\n");
	for (uint i = 0; i < memory_slots; ++i)
	{
		printf("[%08x-%08x] - ", (uint) mmap_entries[i].addr,
			   (uint) (mmap_entries[i].addr + mmap_entries[i].len - 1));
		switch (mmap_entries[i].type)
		{
			case MULTIBOOT_MEMORY_AVAILABLE:
				printf("Available");
				break;
			case MULTIBOOT_MEMORY_RESERVED:
				printf("Reserved");
				break;
			default:
				printf("%u", mmap_entries[i].type);
		}
		printf("\n");
	}
}*/
