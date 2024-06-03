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

/**
 * Writes a character in the framebuffer.
*
* @param i The location in the framebuffer
* @param c The character
* @param bg The foreground color
* @param fg The background color
*/
void fb_write_cell(unsigned int i, char c, unsigned char bg, unsigned char fg);


/** Moves the cursor of the framebuffer to the given position
*
* @param pos The new position of the cursor
*/
void fb_move_cursor(unsigned short pos);

/** Clear the screen */
void fb_clear_screen();

/** Write a string
 *
 * @param n String to print
 */
void fb_write(char* buf);

/** Write an ok decorator */
void fb_ok();

/** Write an error decorator */
void fb_error();

/** Write an error message with red error decorations
 *
 * @param msg Error message to print
 */
void fb_write_error_msg(char* msg);

/** Scroll one line up  */
void fb_scroll();

/** Write an info message with green ok decorations
 *
 * @param msg Info message to print
 */
void fb_write_info_msg(char* msg);

/** Write a character to the framebuffer
 *
 * @param c Character to print
 */
void fb_write_char(char c);

#endif /* INCLUDE_FB_H */