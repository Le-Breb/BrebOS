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

#define FB_WIDTH 80
#define FB_HEIGHT 25

/** fb_write_cell:
* Writes a character with the given foreground and background to position i
* in the framebuffer.
*
* @param i The location in the framebuffer
* @param c The character
* @param fg The foreground color
* @param bg The background color
*/
void fb_write_cell(unsigned int i, char c, unsigned char fg, unsigned char bg);


/** fb_move_cursor:
* Moves the cursor of the framebuffer to the given position
*
* @param pos The new position of the cursor
*/
void fb_move_cursor(unsigned short pos);

/** fb_clear_screen:
 * Clears the screen by filling the franbuffer with spaces with black bg and white fg
*/
void fb_clear_screen();

void fb_write(char* buf);

void fb_write_hex(unsigned int n);

void fb_write_dec(unsigned int n);

void fb_ok();
#endif /* INCLUDE_FB_H */