#include "multiboot.h"

#include "../core/fb.h"
#include "../core/memory/memory.h"

const multiboot_info_t* Multiboot::multiboot_info = nullptr;
bool Multiboot::is_used = false;

void Multiboot::print_mmap()
{
	for (const auto& mmap_entry : get_mmap())
	{
		printf("[0x%08llx-0x%08llx] - ", mmap_entry.addr, mmap_entry.addr + mmap_entry.len);
		switch (mmap_entry.type)
		{
			case MULTIBOOT_MEMORY_AVAILABLE:
				FB::write("AVAILABLE");
				break;
			case MULTIBOOT_MEMORY_RESERVED:
				FB::write("RESERVED");
				break;
			case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
				FB::write("ACPI_RECLAIMABLE");
				break;
			case MULTIBOOT_MEMORY_NVS:
				FB::write("NVS");
				break;
			case MULTIBOOT_MEMORY_BADRAM:
				FB::write("BADRAM");
				break;
			default:
				FB::write("UNKNOWN");
		}

		FB::putchar('\n');
	}
}

vector<multiboot_memory_map_t> Multiboot::get_mmap()
{
	vector<multiboot_memory_map_t> entries;

	if (!is_used)
		irrecoverable_error("%s called while multiboot not used (or not Multiboot::init not called)", __PRETTY_FUNCTION__);

	void* mmap_ptr = get_tag(MULTIBOOT_TAG_TYPE_MMAP);
	if (!mmap_ptr)
	{
		printf_error("%s: cannot find mmap tag", __PRETTY_FUNCTION__);
		return {};
	}

	multiboot_tag_mmap_t* mmap = (multiboot_tag_mmap_t*)mmap_ptr;
	for (multiboot_mmap_entry* mmap_entry = mmap->entries; (uint32_t)((uint8_t*)mmap_entry - (uint8_t*)mmap) < mmap->
		 size; mmap_entry = (multiboot_mmap_entry*)((uint8_t*)mmap_entry + mmap->entry_size))
		entries.push_back(*mmap_entry);

	return entries;
}

void Multiboot::init(const multiboot_info_t* multiboot_info)
{
	Multiboot::multiboot_info = (multiboot_info_t*)multiboot_info;
	is_used = multiboot_info != nullptr;
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