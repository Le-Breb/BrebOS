#include "keyboard.h"
#include "fb.h"
#include "interrupts.h"

char kbd_US[128] =
		{
				0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
				'\t', /* <-- Tab */
				'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
				0, /* <-- control key */
				'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm',
				',', '.', '/', 0,
				'*',
				0,  /* Alt */
				' ',  /* Space bar */
				0,  /* Caps lock */
				0,  /* 59 - F1 key ... > */
				0, 0, 0, 0, 0, 0, 0, 0,
				0,  /* < ... F10 */
				0,  /* 69 - Num lock*/
				0,  /* Scroll Lock */
				0,  /* Home key */
				0,  /* Up Arrow */
				0,  /* Page Up */
				'-',
				0,  /* Left Arrow */
				0,
				0,  /* Right Arrow */
				'+',
				0,  /* 79 - End key*/
				0,  /* Down Arrow */
				0,  /* Page Down */
				0,  /* Insert Key */
				0,  /* Delete Key */
				0, 0, 0,
				0,  /* F11 Key */
				0,  /* F12 Key */
				0,  /* All other keys are undefined */
		};

void keyboard_interrupt_handler()
{
	unsigned char scan_code = read_scan_code();
	char c[2] = {kbd_US[scan_code], 0};
	if ((c[0] >= 'A' && c[0] <= 'Z') || (c[0] >= 'a' && c[0] <= 'z') || c[0] == ' ' || (c[0] >= '1' && c[0] <= '9'))
		fb_write(c);
}