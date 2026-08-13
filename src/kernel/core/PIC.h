#ifndef CUSTOM_OS_PIC_H
#define CUSTOM_OS_PIC_H

#include <kstddef.h>
#include <stdint.h>

#define ICW1_INIT    0x10
#define ICW1_ICW4    0x01
#define ICW4_8086    0x01

#define PIC1         0x20    // IO base address for master PIC
#define PIC2         0xA0    // IO base address for slave PIC
#define PIC_READ_ISR 0x0B
#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2 + 1)

/* The PIC interrupts have been remapped */
#define PIC1_START_INTERRUPT 0x20
#define PIC2_START_INTERRUPT 0x28
#define PIC2_END_INTERRUPT (PIC2_START_INTERRUPT + 7)

#define PIC_ACK     0x20

class PIC
{
public:
	static bool preemptive_scheduling_enabled;

	/**
	 * Sets up the PIC
	 */
	static void init();

	/**
	 * Remaps the PIC interrupts
	 *
	 * @param offset1 The offset for the first PIC
	 * @param offset2 The offset for the second PIC
	 */
	static void remap(int offset1, int offset2);

	/**
	 *  Acknowledges an interrupt from either PIC 1 or PIC 2.
	 *
	 *  @param num The number of the interrupt
	 */
	static void acknowledge(uint interrupt);

	static void enable_preemptive_scheduling();

	static void disable_preemptive_scheduling();

	/**
	 * Unmasks a single IRQ line (0-15) so the PIC actually forwards it to the CPU.
	 * init() only unmasks the lines known to be needed at boot (keyboard, cascade,
	 * the hardcoded networking IRQ) - any other device's IRQ line must be unmasked
	 * individually once it's known, otherwise its interrupts are silently dropped
	 * at the PIC regardless of IDT/dispatch table setup.
	 *
	 * @param irq_line The IRQ line to unmask (0-7 = master PIC, 8-15 = slave PIC)
	 */
	static void unmask_irq(uint8_t irq_line);

	static bool is_spurious(int interrupt);
};


#endif //CUSTOM_OS_PIC_H
