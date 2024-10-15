#include "fb.h"
#include "io.h"
#include "clib/string.h"

unsigned int caret_pos = 0; /* Framebuffer index */
short* const fb = (short*) FB_ADDR;
unsigned char BG = FB_BLACK;
unsigned char FG = FB_WHITE;

void FB::scroll()
{
	// Offset lines
	for (int i = 0; i < FB_WIDTH * (FB_HEIGHT - 1); ++i)
		fb[i] = fb[i + FB_WIDTH];
	// Clear last line
	for (int i = 0; i < FB_WIDTH; i++)
		fb[FB_WIDTH * (FB_HEIGHT - 1) + i] = ' ';
}

void FB::write_cell(char c)
{
	// Scroll if buffer full
	while (caret_pos >= FB_WIDTH * FB_HEIGHT)
	{
		scroll();
		caret_pos -= FB_WIDTH;
	}
	if (c == '\n')
	{
		caret_pos = (caret_pos / FB_WIDTH + 1) * FB_WIDTH;
		return;
	}

	// Write to fb
	fb[caret_pos++] = (short) ((((BG & 0x0F) << 4) | (FG & 0x0F)) << 8 | c);
}

void FB::move_cursor(unsigned short pos)
{
	outb(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);   /* Send pos high bits command */
	outb(FB_DATA_PORT, ((pos >> 8) & 0x00FF));     /* Send pos high bits */
	outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);    /* Send pos low bits command */
	outb(FB_DATA_PORT, pos & 0x00FF);              /* Send pos low bits */
}

void FB::clear_screen()
{
	caret_pos = 0;
	for (int i = 0; i < FB_HEIGHT * FB_WIDTH; i++)
		write_cell(' ');
	caret_pos = 0;
}

void FB::write(const char* buf)
{
	for (unsigned int i = 0; i < strlen(buf); i++)
		write_cell(buf[i]);
}

void FB::ok()
{
	ok_decorator();
	write_cell('\n');
}

void FB::error()
{
	error_decorator();
	write_cell('\n');
}

void FB::info()
{
	info_decorator();
	write_cell('\n');
}

void FB::ok_decorator()
{
	write_cell('[');
	set_fg(FB_GREEN);
	write("OK");
	set_fg(FB_WHITE);
	write_cell(']');
}

void FB::error_decorator()
{
	write_cell('[');
	set_fg(FB_RED);
	write("ERROR");
	set_fg(FB_WHITE);
	write_cell(']');
}

void FB::info_decorator()
{
	write_cell('[');
	set_fg(FB_BLUE);
	write("INFO");
	set_fg(FB_WHITE);
	write_cell(']');
}

void FB::set_fg(unsigned char fg)
{
	FG = fg;
}

void FB::set_bg(unsigned char bg)
{
	BG = bg;
}