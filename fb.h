#ifndef INCLUDE_FB_H
#define INCLUDE_FB_H

/* The I/O ports */
#define FB_COMMAND_PORT 0x3D4
#define FB_DATA_PORT 0x3D5

/* The I/O port commands */
#define FB_HIGH_BYTE_COMMAND 14
#define FB_LOW_BYTE_COMMAND 15
#define FB_GREEN 2
#define FB_DARK_GREY 8
#define FB_BLACK 0
#define FB_WHITE 15
#define FB_RED 4
#define FB_BLUE 1

#define FB_WIDTH 80
#define FB_HEIGHT 25

#define FB_ADDR 0xC03FF000

class FB
{
	/**
	 * Set foreground color
	 * @param fg foreground code
	 */
	static void set_fg(unsigned char fg);

	/**
	 * Set background color
	 * @param bg foreground code
	 */
	static void set_bg(unsigned char bg);

public:
	/**
	 * Writes a character in the framebuffer at current cursor position. Scroll if needed.
	*
	* @param c The character
	*/
	static void write_cell(char c);


	/** Moves the cursor of the framebuffer to the given position
	*
	* @param pos The new position of the cursor
	*/
	static void move_cursor(unsigned short pos);

	/** Clear the screen */
	static void clear_screen();

	/** Write a string
	 *
	 * @param buf String to print
	 */
	static void write(const char* buf);

	/** Write an ok decorator without trailing newline */
	static void ok_decorator();

	/** Write an error decorator without trailing newline */
	static void error_decorator();

	/** Write an info decorator without trailing newline*/
	static void info_decorator();

	/** Write an ok decorator */
	static void ok();

	/** Write an error decorator */
	static void error();

	/** Write an info decorator */
	static void info();

	/** Scroll one line up  */
	static void scroll();
};

#endif /* INCLUDE_FB_H */