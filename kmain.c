#include "fb.h"
#include "gdt.h"
#include "interrupts.h"
#include "multiboot.h"
#include "memory.h"

gdt_entry_t gdt[GDT_ENTRIES];
gdt_descriptor_t gdt_descriptor;

struct idt_entry idt[256];
struct idt_descriptor idt_descriptor;

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

	fb_write("Initialize memory\n");
	init_mem((multiboot_info_t*) (ebx + 0xC0000000));
	fb_ok();

	// Do stuff
	/*int* a = malloc(10);
	*a = 2;
	int* b = malloc(20);
	*b = 2;

	free(b);
	free(a);*/

	run_module(0);

	//fb_clear_screen();
	//fb_write("End of kernel\n");

	return 0;
}