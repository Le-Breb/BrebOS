#include "fb.h"
#include "gdt.h"
#include "interrupts.h"
#include "multiboot.h"
#include "memory.h"
#include "system.h"
#include "process.h"

gdt_entry_t gdt[GDT_ENTRIES];
gdt_descriptor_t gdt_descriptor;

struct idt_entry idt[256];
struct idt_descriptor idt_descriptor;

#include "lib/stdio.h"

//Todo: Return pages to the page frame allocator when enough memory is freed
//Todo: Investigate about whether page 184 (VGA buffer start address is mapped twice)
//Todo: same for page pde 771 pte11 at the end of run_module
int kmain([[maybe_unused]] unsigned int ebx)
{
	disable_interrupts();

	// Initialize framebuffer
	fb_clear_screen();

	// Setup GDT for segmentation
	printf("Setting up GDT\n");
	gdt_init(&gdt_descriptor, gdt);
	fb_ok();

	// Setup IDT for interrupts
	printf("Remapping PIC\n");
	setup_pic();
	fb_ok();
	printf("Setting up IDT\n");
	idt_init(&idt_descriptor, idt);
	fb_ok();
	printf("Enabling interrupts\n");
	enable_interrupts();
	fb_ok();

	printf("Initialize memory\n");
	init_mem((multiboot_info_t*) (ebx + 0xC0000000));
	fb_ok();

	// Do stuff
	/*int* a = malloc(10);
	*a = 2;
	int* b = malloc(20);
	*b = 2;

	free(b);
	free(a);*/

	run_module(0, get_grub_modules(), get_page_tables());
	//fb_clear_screen();

	return shutdown();
}