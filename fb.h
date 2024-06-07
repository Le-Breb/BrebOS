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
void fb_write_cell(char c);


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

/** Write an info decorator */
void fb_info();

/** Write an ok decorator without trailing newline */
void fb_ok_();

/** Write an error decorator without trailing newline */
void fb_error_();

/** Write an info decorator without trailing newline*/
void fb_info_();

/** Scroll one line up  */
void fb_scroll();

/** Write a character to the framebuffer
 *
 * @param c Character to print
 */
void fb_write_char(char c);

/**
 * Set foreground color
 * @param fg foreground code
 */
void fb_set_fg(unsigned char fg);

/**
 * Set background color
 * @param bg foreground code
 */
void fb_set_bg(unsigned char bg);

#endif /* INCLUDE_FB_H */