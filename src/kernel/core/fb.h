#ifndef INCLUDE_FB_H
#define INCLUDE_FB_H

/* The I/O ports */
#include <kstddef.h>

#define FB_COMMAND_PORT 0x3D4
#define FB_DATA_PORT 0x3D5

/* The I/O port commands */
#define FB_HIGH_BYTE_COMMAND 14
#define FB_LOW_BYTE_COMMAND 15
#define FB_BLACK        0x00
#define FB_BLUE         0x01
#define FB_GREEN        0x02
#define FB_CYAN         0x03
#define FB_RED          0x04
#define FB_MAGENTA      0x05
#define FB_BROWN        0x06
#define FB_LIGHTGREY    0x07
#define FB_LIGHTGRAY    0x07
#define FB_DARKGREY     0x08
#define FB_DARKGRAY     0x08
#define FB_LIGHTBLUE    0x09
#define FB_LIGHTGREEN   0x0A
#define FB_LIGHTCYAN    0x0B
#define FB_LIGHTRED     0x0C
#define FB_LIGHTMAGENTA 0x0D
#define FB_YELLOW       0x0E
#define FB_WHITE        0x0F

#define FB_WIDTH 80
#define FB_HEIGHT 25

#define FB_ADDR 0xC03FF000

#define CURSOR_END_LINE 0x0B
#define CURSOR_BEGIN_LINE 0x0A

class FB
{
    static uint caret_pos; /* Framebuffer index */
    static short* const fb;
    static unsigned char BG;
    static unsigned char FG;

public:
    static void init();

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

    /**
     * Writes a character in the framebuffer at current cursor position. Scroll if needed.
    *
    * @param c The character
    */
    static void putchar(char c);


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

    /** Write an warm decorator without trailing newline*/
    static void warn_decorator();

    /** Write an ok decorator */
    static void ok();

    /** Write an error decorator */
    static void error();

    /** Write an info decorator */
    static void info();

    /** Write an info decorator */
    static void warn();

    /** Scroll one line up  */
    static void scroll();

    /** Delete a character */
    static void delchar();
};

/**
 * Prints a formatted string the same way the usual printf would.
 * This function has the same name as in libc, which the kernel is linked to.
 * However, libc printf uses a syscall to display a character, while this function doesn't.
 * Hopefully, there is no compilation conflict, and kernel calls to printf execute this function, not the one in libc.
 * This function is way faster than the libc version as it bypasses syscalls (we're already in the kernel, no need for
 * syscalls).
 * If libc printf was called, this would be very bad as we'd have syscalls calling syscalls (a syscall that calls printf
 * which uses syscalls). This would be very poor and stupid in terms of performance, and would probably not run
 * properly (I guess ?)
 */
__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...);

__attribute__ ((format (printf, 1, 2))) int printf_error(const char* format, ...);

__attribute__ ((format (printf, 1, 2))) int printf_info(const char* format, ...);

[[noreturn]]
__attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...);

__attribute__ ((format (printf, 1, 2))) int printf_warn(const char* format, ...);

#endif /* INCLUDE_FB_H */
