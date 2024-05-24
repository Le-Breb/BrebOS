#include "fb.h"
#include "gdt.h"
#include "interrupts.h"

struct gdt_entry gdt[3];
struct gdt gdt_descriptor;

struct idt_entry idt[256];
struct idt idt_descriptor;

int kmain()
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

	// Do stuff
	//fb_write_cell(0, 'A', FB_BLACK, FB_WHITE);
	fb_move_cursor(1);

	return 0;
}