#include "multiboot.h"
#include "libc/stddef.h"
#include "libc/string.h"
#include "libc/stdio.h"

void Multiboot::print_mmap(uint ebx)
{
	multiboot_info* mboot_header = (multiboot_info*) (ebx + 0xC0000000);
	uint memory_slots = mboot_header->mmap_length / sizeof(multiboot_memory_map_t);

	uint mem_map_start = (uint) mboot_header->mmap_addr + 0xC0000000;
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
}