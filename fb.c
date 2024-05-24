#include "fb.h"
#include "io.h"

int position = 0;

void fb_write_cell(unsigned int i, char c, unsigned char fg, unsigned char bg)
{
    if (c == '\n')
    {
        position = (position / 80 + 1) * 80;
        return;
    }

    short* const fb = (short*)0x000B8000;
    fb[i] = (((fg & 0x0F) << 4) | (bg & 0x0F)) << 8 | c;
}

void fb_move_cursor(unsigned short pos)
{
    outb(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);
    outb(FB_DATA_PORT, ((pos >> 8) & 0x00FF));
    outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);
    outb(FB_DATA_PORT, pos & 0x00FF);
}

void fb_clear_screen()
{
    for (int i = 0; i < FB_HEIGHT; i++)
        for (int j = 0; j < FB_WIDTH; j++)
            fb_write_cell(i * 80 + j, ' ', FB_BLACK, FB_WHITE);    
}

unsigned int strlen(char* str)
{
    unsigned int len = 0;
    while (str[len])
        len++;
    return len;
}

void fb_write(char* buf)
{
    for (unsigned int i = 0; i < strlen(buf); i++)
        fb_write_cell(position++, buf[i], FB_BLACK, FB_WHITE);
}

void fb_ok()
{
    fb_write_cell(position++, '[', FB_BLACK, FB_WHITE);
    fb_write_cell(position++, 'O', FB_BLACK, FB_GREEN);
    fb_write_cell(position++, 'K', FB_BLACK, FB_GREEN);
    fb_write_cell(position++, ']', FB_BLACK, FB_WHITE);
    fb_write_cell(position++, '\n', FB_BLACK, FB_WHITE);
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
        {fb_write("0");return;}

    char buf[11];
    buf[10] = 0;
    int i = 9;
    while (n)
    {
        buf[i--] = '0' + n % 10;
        n /= 10;
    }
    fb_write(buf + i + 1);
}