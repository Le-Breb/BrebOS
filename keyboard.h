#ifndef INCLUDE_KEYBOARD_H
#define INCLUDE_KEYBOARD_H

#define KBD_DATA_PORT   0x60

class Keyboard
{
	static char kbd_US[128];
public:
	static void interrupt_handler();

	/**
	 *  Reads a scan code from the keyboard
	 *
	 *  @return The scan code (NOT an ASCII character!)
	 */
	static unsigned char read_scan_code();
};

#endif //INCLUDE_KEYBOARD_H
