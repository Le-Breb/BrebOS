#include "fb.h"
#include "io.h"

int caret_pos = 0; /* Framebuffer index */
short* const fb = (short*) FB_ADDR;

void fb_scroll()
{
	// Offset lines
	for (int i = 0; i < FB_WIDTH * (FB_HEIGHT - 1); ++i)
		fb[i] = fb[i + FB_WIDTH];
	// Clear last line
	for (int i = 0; i < FB_WIDTH; i++)
		fb[FB_WIDTH * (FB_HEIGHT - 1) + i] = ' ';
}

void fb_write_cell(unsigned int i, char c, unsigned char fg, unsigned char bg)
{
	// Scroll if buffer full
	if (i == FB_WIDTH * FB_HEIGHT)
	{
		fb_scroll();
		i -= FB_WIDTH;
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
	fb[i] = (short) ((((fg & 0x0F) << 4) | (bg & 0x0F)) << 8 | c);
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
	for (int i = 0; i < FB_HEIGHT * FB_WIDTH; i++)
		fb_write_cell(i, ' ', FB_BLACK, FB_WHITE);
}

unsigned int strlen(const char* str)
{
	unsigned int len = 0;
	while (str[len])
		len++;
	return len;
}

void fb_write(char* buf)
{
	for (unsigned int i = 0; i < strlen(buf); i++)
		fb_write_cell(caret_pos++, buf[i], FB_BLACK, FB_WHITE);
}

void fb_ok()
{
	fb_write_cell(caret_pos++, '[', FB_BLACK, FB_WHITE);
	fb_write_cell(caret_pos++, 'O', FB_BLACK, FB_GREEN);
	fb_write_cell(caret_pos++, 'K', FB_BLACK, FB_GREEN);
	fb_write_cell(caret_pos++, ']', FB_BLACK, FB_WHITE);
	fb_write_cell(caret_pos++, '\n', FB_BLACK, FB_WHITE);
}

void fb_error()
{
	fb_write_cell(caret_pos++, '[', FB_BLACK, FB_WHITE);
	fb_write_cell(caret_pos++, 'E', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'O', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, ']', FB_BLACK, FB_WHITE);
	fb_write_cell(caret_pos++, '\n', FB_BLACK, FB_WHITE);
}

void fb_write_hex(unsigned int n)
{
	char buf[9];
	buf[8] = 0;
	for (int i = 7; i >= 0; i--)
	{
		buf[i] = "0123456789ABCDEF"[n & 0xF];
		n >>= 4;
	}
	fb_write(buf);
}

void fb_write_dec(unsigned int n)
{
	if (n == 0)
	{
		fb_write("0");
		return;
	}

	char buf[11];
	buf[10] = 0;
	int i = 9;
	while (n)
	{
		buf[i--] = (char) ('0' + n % 10);
		n /= 10;
	}
	fb_write(buf + i + 1);
}

void fb_write_error_msg(char* msg)
{
	fb_write_cell(caret_pos++, '[', FB_BLACK, FB_WHITE);
	fb_write_cell(caret_pos++, 'E', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'O', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, ']', FB_BLACK, FB_WHITE);
	fb_write_cell(caret_pos++, ' ', FB_BLACK, FB_WHITE);

	fb_write(msg);
	fb_write_cell(caret_pos++, '[', FB_BLACK, FB_WHITE);
	fb_write_cell(caret_pos++, 'E', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'O', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, 'R', FB_BLACK, FB_RED);
	fb_write_cell(caret_pos++, ']', FB_BLACK, FB_WHITE);
	fb_write_cell(caret_pos++, '\n', FB_BLACK, FB_WHITE);
}
