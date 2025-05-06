#ifndef INCLUDE_FB_H
#define INCLUDE_FB_H

/* The I/O ports */
#include <kstddef.h>

#include "kstdint.h"
#include "../utils/PSF.h"

#define FB_BLACK        0x000000
#define FB_BLUE         0x0000AA
#define FB_GREEN        0x00AA00
#define FB_CYAN         0x00AAAA
#define FB_RED          0xAA0000
#define FB_MAGENTA      0xAA00AA
#define FB_BROWN        0xAA5500
#define FB_LIGHTGREY    0xAAAAAA
#define FB_LIGHTGRAY    0xAAAAAA
#define FB_DARKGREY     0x555555
#define FB_DARKGRAY     0x555555
#define FB_LIGHTBLUE    0x5555FF
#define FB_LIGHTGREEN   0x55FF55
#define FB_LIGHTCYAN    0x55FFFF
#define FB_LIGHTRED     0xFF5555
#define FB_LIGHTMAGENTA 0xFF55FF
#define FB_YELLOW       0xFFFF55
#define FB_WHITE        0xFFFFFF

#define FB_ADDR 0xC03FF000

#define CURSOR_END_LINE 0x0B
#define CURSOR_BEGIN_LINE 0x0A

#define WHITE 0xFFFFFF
#define BLACK 0

#define FLUSH_LOCKED_ACTION(ret, action) {\
FB::lock_flushing(); \
(ret) = (action); \
FB::unlock_flushing(); \
}

typedef unsigned char uchar;

class FB
{
    struct cell
    {
        uint32_t fb, bg;
        char c;
    };

    static uint32_t* fb;

    static uint caret_x, caret_y;
    static uint32_t FG, BG;

    static uint fb_width, fb_height, fb_pitch;
    static uint n_row;
    static uint characters_per_line;
    static uint characters_per_col;

    static uint dirty_start_y, dirty_end_y;
    static uint dirty_start_x, dirty_end_x;

    static uint fps;

    static cell* fb_shadow;
    static uint shadow_lim;
    static uint shadow_start;

    static bool lock_flush;
    static bool show_cursor;

    static PSF1_font* font;
    static uint32_t* r_font;
    static char* glyphs;

    static uint progress_bar_height;
    static char progress_bar_percentage;
    static const void* progress_bar_owner;
    static constexpr char progress_char = (char)129;

    static void update_dirty_rect();
public:
    /**
     * Continuously refreshes the framebuffer (ie the display)
     * @details Draws the cursor, the progress bar, and the dirty rectangle
     */
    [[noreturn]]
    static void refresh_loop();

    static void flush();

    static void lock_flushing();

    static void unlock_flushing();

    static void init(uint fps);

    /**
     * Set foreground color
     * @param fg foreground code
     */
    static void set_fg(uint32_t fg);

    /**
     * Set background color
     * @param bg foreground code
     */
    static void set_bg(uint32_t bg);


    /**
     * Writes a character in the framebuffer at current cursor position. Scroll if needed.
    *
    * @param c The character
    */
    static void putchar(unsigned short int c);

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
    static void scroll(uint n);

    /** Delete a character */
    static void delchar();

    /**
     * Updates the percentage displayed by the progress bar
     * @param new_percentage new percentage to display
     * @param id unique identifier
     */
    static void update_progress_bar(char new_percentage, const void* id);

    /**
     * Try to obtain ownership over the progress bar. If the bar is used, does nothing. Otherwise, the bar owner is
     * set to id. This id will be needed for any further interaction with the bar.
     * @param id unique identifier
     * @return whether bar was successfully acquired
     */
    static bool try_acquire_progress_bar(const void* id);

    /**
     * Indicate that we do not need the progress bar anymore.
     * @param id unique identifier used to acquire the bar
     * @note This function can be called even if try_acquire_progress_bar failed
     */
    static void release_progres_bar(const void* id);
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
