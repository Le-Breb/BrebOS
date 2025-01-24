#include "PIC.h"
#include "IO.h"

void PIC::init()
{
	// Remap the PIC
	remap(PIC1_START_INTERRUPT, PIC2_START_INTERRUPT);

	// Only enable keyboard interrupts
	outb(PIC1_DATA, ~0x02);
	outb(PIC2_DATA, ~0x00);
	io_wait();
}

void PIC::remap(int offset1, int offset2)
{
	unsigned char a1, a2;

	// Save masks
	a1 = inb(PIC1_DATA);
	a2 = inb(PIC2_DATA);

	// Start initialization sequence in cascade mode
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();  // Give PICs time to process the command

	// Set new vector offset
	outb(PIC1_DATA, offset1);
	outb(PIC2_DATA, offset2);
	io_wait();

	// Configure master PIC to recognize there is a slave PIC at IRQ2 (0000 0100)
	outb(PIC1_DATA, 4);
	outb(PIC2_DATA, 2);
	io_wait();

	// Set PICs to 8086/88 mode
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC2_DATA, ICW4_8086);
	io_wait();

	// Restore saved masks
	outb(PIC1_DATA, a1);
	outb(PIC2_DATA, a2);
	io_wait();
}

void PIC::acknowledge(uint interrupt)
{
	if (interrupt < PIC1_START_INTERRUPT || interrupt > PIC2_END_INTERRUPT)
		return;

	outb(interrupt < PIC2_START_INTERRUPT ? PIC1 : PIC2, PIC_ACK);
}

void PIC::enable_preemptive_scheduling()
{
	uint a = inb(PIC1_DATA) & ~1;
	outb(PIC1_DATA, a);
}

void PIC::disable_preemptive_scheduling()
{
	uint a = inb(PIC1_DATA) | 1;
	outb(PIC1_DATA, a);
}