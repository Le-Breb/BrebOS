#include "fb.h"
#include "io.h"
#include "lib/string.h"

unsigned int caret_pos = 0; /* Framebuffer index */
short* const fb = (short*) FB_ADDR;
unsigned char BG = FB_BLACK;
unsigned char FG = FB_WHITE;

void fb_scroll()
{
	// Offset lines
	for (int i = 0; i < FB_WIDTH * (FB_HEIGHT - 1); ++i)
		fb[i] = fb[i + FB_WIDTH];
	// Clear last line
	for (int i = 0; i < FB_WIDTH; i++)
		fb[FB_WIDTH * (FB_HEIGHT - 1) + i] = ' ';
}

void fb_write_cell(char c)
{
	// Scroll if buffer full
	if (caret_pos == FB_WIDTH * FB_HEIGHT)
	{
		fb_scroll();
		caret_pos -= FB_WIDTH;
	}
	if (c == '\n')
	{
		caret_pos = (caret_pos / FB_WIDTH + 1) * FB_WIDTH;

		// Scroll if necessary
		if (caret_pos >= FB_WIDTH * FB_HEIGHT)
		{
			fb_scroll();
			caret_pos -= FB_WIDTH;
		}
		return;
	}

	// Write to fb
	fb[caret_pos++] = (short) ((((BG & 0x0F) << 4) | (FG & 0x0F)) << 8 | c);
}

void fb_move_cursor(unsigned short pos)
{
	outb(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);   /* Send pos high bits command */
	outb(FB_DATA_PORT, ((pos >> 8) & 0x00FF));     /* Send pos high bits */
	outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);    /* Send pos low bits command */
	outb(FB_DATA_PORT, pos & 0x00FF);              /* Send pos low bits */
}

void fb_clear_screen()
{
	caret_pos = 0;
	for (int i = 0; i < FB_HEIGHT * FB_WIDTH; i++)
		fb_write_cell(' ');
	caret_pos = 0;
}

void fb_write(char* buf)
{
	for (unsigned int i = 0; i < strlen(buf); i++)
		fb_write_cell(buf[i]);
}

void fb_ok()
{
	fb_ok_();
	fb_write_cell('\n');
}

void fb_error()
{
	fb_error_();
	fb_write_cell('\n');
}

void fb_info()
{
	fb_info_();
	fb_write_cell('\n');
}

void fb_ok_()
{
	fb_write_cell('[');
	fb_set_fg(FB_GREEN);
	fb_write("OK");
	fb_set_fg(FB_WHITE);
	fb_write_cell(']');
}

void fb_error_()
{
	fb_write_cell('[');
	fb_set_fg(FB_RED);
	fb_write("ERROR");
	fb_set_fg(FB_WHITE);
	fb_write_cell(']');
}

void fb_info_()
{
	fb_write_cell('[');
	fb_set_fg(FB_BLUE);
	fb_write("INFO");
	fb_set_fg(FB_WHITE);
	fb_write_cell(']');
}

void fb_write_char(char c)
{
	fb_write_cell(c);
}

void fb_set_fg(unsigned char fg)
{
	FG = fg;
}

void fb_set_bg(unsigned char bg)
{
	BG = bg;
}