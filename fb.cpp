#include "fb.h"
#include "IO.h"
#include "libc/string.h"

uint FB::caret_pos = 0;
short* const FB::fb = (short*) FB_ADDR;
unsigned char FB::BG = FB_BLACK;
unsigned char FB::FG = FB_WHITE;

void FB::scroll()
{
	// Offset lines
	for (int i = 0; i < FB_WIDTH * (FB_HEIGHT - 1); ++i)
		fb[i] = fb[i + FB_WIDTH];
	// Clear last line
	for (int i = 0; i < FB_WIDTH; i++)
		fb[FB_WIDTH * (FB_HEIGHT - 1) + i] = ((((BG & 0x0F) << 4) | (FG & 0x0F)) << 8 | ' ');
}

void FB::delchar()
{
	if (!caret_pos)
		return;
	caret_pos--;
	putchar(' ');
	caret_pos--;
	move_cursor(caret_pos);
}

void FB::putchar(char c)
{
	if (c == '\b')
	{
		delchar();
		return;
	}
	if (c == '\n')
	{
		caret_pos = (caret_pos / FB_WIDTH + 1) * FB_WIDTH;
		move_cursor(caret_pos + 1);

		return;
	}
	// Scroll if buffer full
	while (caret_pos >= FB_WIDTH * FB_HEIGHT)
	{
		scroll();
		caret_pos -= FB_WIDTH;
		move_cursor(caret_pos);
	}

	// Write to fb
	move_cursor(caret_pos + 1);
	fb[caret_pos++] = (short) ((((BG & 0x0F) << 4) | (FG & 0x0F)) << 8 | c);
}

void FB::move_cursor(unsigned short pos)
{
	if (pos >= 2000)
		return;
	outb(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND); /* Send pos high bits command */
	outb(FB_DATA_PORT, ((pos >> 8) & 0x00FF)); /* Send pos high bits */
	outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND); /* Send pos low bits command */
	outb(FB_DATA_PORT, pos & 0x00FF); /* Send pos low bits */
}

void FB::clear_screen()
{
	caret_pos = 0;
	for (int i = 0; i < FB_HEIGHT * FB_WIDTH; i++)
		putchar(' ');
	caret_pos = 0;
}

void FB::write(const char* buf)
{
	for (uint i = 0; i < strlen(buf); i++)
		putchar(buf[i]);
}

void FB::ok()
{
	ok_decorator();
	putchar('\n');
}

void FB::error()
{
	error_decorator();
	putchar('\n');
}

void FB::info()
{
	info_decorator();
	putchar('\n');
}

void FB::ok_decorator()
{
	putchar('[');
	set_fg(FB_GREEN);
	write("OK");
	set_fg(FB_WHITE);
	putchar(']');
}

void FB::error_decorator()
{
	putchar('[');
	set_fg(FB_RED);
	write("ERROR");
	set_fg(FB_WHITE);
	putchar(']');
}

void FB::info_decorator()
{
	putchar('[');
	set_fg(FB_BLUE);
	write("INFO");
	set_fg(FB_WHITE);
	putchar(']');
}

void FB::set_fg(unsigned char fg)
{
	FG = fg;
}

void FB::set_bg(unsigned char bg)
{
	BG = bg;
}

void FB::init()
{
	outb(FB_COMMAND_PORT, CURSOR_END_LINE); // set the cursor end line to 15
	outb(FB_DATA_PORT, 0x0F);

	outb(FB_COMMAND_PORT, CURSOR_BEGIN_LINE); // set the cursor start line to 14 and enable cursor visibility
	outb(FB_DATA_PORT, 0x0E);
	clear_screen();
}
