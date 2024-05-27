#include "fb.h"
#include "gdt.h"
#include "interrupts.h"
#include "multiboot.h"
#include "memory.h"

struct gdt_entry gdt[3];
struct gdt gdt_descriptor;

struct idt_entry idt[256];
struct idt idt_descriptor;

//Todo: Return pages to the page frame allocator when enough memory is freed
int kmain([[maybe_unused]] unsigned int ebx)
{
	disable_interrupts();

	// Initialize framebuffer
	fb_clear_screen();

	// Setup GDT for segmentation
	fb_write("Setting up GDT\n");
	gdt_init(&gdt_descriptor, gdt);
	fb_ok();

	// Setup IDT for interrupts
	fb_write("Remapping PIC\n");
	setup_pic();
	fb_ok();
	fb_write("Setting up IDT\n");
	idt_init(&idt_descriptor, idt);
	fb_ok();
	fb_write("Enabling interrupts\n");
	enable_interrupts();
	fb_ok();

	fb_write("Initialize page bitmap\n");
	init_mem();
	fb_ok();

	// Do stuff
	int* a = malloc(10);
	*a = 2;
	int* b = malloc(20);
	*b = 2;

	free(b);
	free(a);

	/*multiboot_info_t *mbi = (multiboot_info_t *)ebx;
	fb_write("Module loaded\n");
	if (mbi->flags & MULTIBOOT_INFO_MODS && mbi->mods_count > 0)
	{
		fb_ok();
		typedef void (* call_module_t)(void);
		multiboot_module_t* address_of_module = (multiboot_module_t*)(mbi->mods_addr + 0xC0000000);

		// Set module page as present
		extern void boot_page_table1();
		unsigned int* pt1 = (unsigned int*)boot_page_table1;
		unsigned int page = (unsigned int)address_of_module->mod_start / 4096;
		pt1[page] = page << 12 | 0x3; // Physical address << 12 | present | writable
		call_module_t start_program = (call_module_t) address_of_module->mod_start + 0xC0000000;
		start_program();
	}
	else
		fb_error();*/

	//fb_clear_screen();
	//fb_write("End of kernel\n");

	return 0;
}