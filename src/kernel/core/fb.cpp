#include "fb.h"

#include <kstdio.h>
#include <kctype.h>
#include <kwchar.h>
#include <kstring.h>

#include "memory.h"
#include "PIT.h"
#include "system.h"

extern char _binary_Lat15_VGA16_psf_start[];

uint FB::fb_width = 0;
uint FB::fb_height = 0;
uint FB::fb_pitch = 0;
uint FB::caret_x = 0;
uint FB::caret_y = 0;
uint FB::characters_per_line = 0;
uint FB::characters_per_col = 0;
uint FB::dirty_start_y = 0;
uint FB::dirty_start_x = 0;
uint FB::dirty_end_y = 0;
uint FB::dirty_end_x = 0;
uint FB::shadow_start = 0;
uint FB::fps = 0;
uint32_t* FB::fb = nullptr;
uint32_t FB::FG = FB_WHITE;
uint32_t FB::BG = FB_BLACK;
bool FB::lock_flush = false;
const void* FB::progress_bar_owner = nullptr;
char FB::progress_bar_percentage = 0;
uint FB::n_row = 0;
uint FB::progress_bar_height = 0;
uint32_t* FB::r_font = nullptr;
PSF1_font* FB::font = (PSF1_font*)_binary_Lat15_VGA16_psf_start;
char* FB::glyphs = (char*)(font + 1);
uint FB::shadow_lim = 0;

// Copy of framebuffer for double buffering. Whenever we want to read some pixel, we use this buffer instead of fb.
// That way, we gain a lot ot time :D
FB::cell* FB::fb_shadow = nullptr;

void FB::scroll(uint n)
{
	shadow_start = (shadow_start + n * fb_width) % shadow_lim;
	if (shadow_start >= shadow_lim)
		shadow_start = shadow_lim;

	// Clear the last n lines
	uint shadow_row_base_off = shadow_start + (fb_height - n) * fb_width;
	for (uint y = 0; y < n; y++)
	{
		auto shadow_off = shadow_row_base_off + y * fb_width;
		if (shadow_off >= shadow_lim)
			shadow_off -= shadow_lim;

		cell* shadow_row = fb_shadow + shadow_off;
		for (uint x = 0; x < fb_width; x++)
			shadow_row[x].c = '\0';
	}

	caret_y -= n;
	dirty_start_y = dirty_start_x = 0;
	dirty_end_y = characters_per_col;
	dirty_end_x = characters_per_line;
}

void FB::delchar()
{
    if (!caret_x && !caret_y)
        return;
    caret_x--;
    putchar(' ');
	caret_x--;
}

void FB::update_progress_bar(char new_percentage, const void* id)
{
	if (id != progress_bar_owner)
		return;

	progress_bar_percentage = new_percentage;
}

bool FB::try_acquire_progress_bar(const void* id)
{
	if (progress_bar_owner)
		return false;

	progress_bar_owner = id;
	return true;
}

void FB::release_progres_bar(const void* id)
{
	if (progress_bar_owner != id)
		return;

	progress_bar_owner = nullptr;
}

#define PUTCHAR_PIXEL(fb_dst, shadow_dst, col, px_x) \
{\
fb_dst[px_x] = col;\
shadow_dst[px_x] = col;\
}


void FB::putchar(
	/* note that this is int, not char as it's a unicode character */
	unsigned short int c)
{
	// Write default character is not ASCII
	if (c >= 255)
		c = 0xDB;

	char cc = (char)(c & 0xFF);

	// Delete character
	if (c == '\b')
	{
		delchar();
		return;
	}

	// Handle line break
	if (c == '\n')
	{
		update_dirty_rect();
		caret_y++;
		caret_x = 0;

		// Scroll if buffer full
		if (caret_y >= characters_per_col)
			scroll(1);

		return;
	}

	update_dirty_rect();

	// Write character
	auto shadow_off = shadow_start + caret_y * fb_width + caret_x;
	if (shadow_off >= shadow_lim)
		shadow_off -= shadow_lim;
	fb_shadow[shadow_off] = {FG, BG, cc};

	// Advance to next character
	caret_x++;

	// Go to next line if needed
	if (caret_x == characters_per_line)
	{
		caret_y++;
		caret_x = 0;

		// Scroll if buffer full
		if (caret_y >= characters_per_col)
			scroll(1);
		else
			update_dirty_rect();
	}
	else
		update_dirty_rect();
}

void FB::lock_flushing()
{
	lock_flush = true;
}

void FB::unlock_flushing()
{
	lock_flush = false;
	flush();
}

void FB::clear_screen()
{
    uint idx = 0;
    for (uint y = 0; y < fb_height; y++)
	    for (uint x = 0; x < fb_width; x++)
		    fb_shadow[idx++] = {FG, BG, '\0'};

    caret_x = caret_y = 0;
	dirty_start_x = dirty_start_y = 0;
	dirty_end_x = characters_per_line;
	dirty_end_y = characters_per_col;
}

void FB::write(const char* buf)
{
	auto len = strlen(buf);
	for (uint i = 0; i < len; i++)
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

void FB::warn()
{
    warn_decorator();
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

void FB::warn_decorator()
{
    putchar('[');
    set_fg(FB_BROWN);
    write("WARN");
    set_fg(FB_WHITE);
    putchar(']');
}

void FB::set_fg(uint32_t fg)
{
    FG = fg;
}

void FB::set_bg(uint32_t bg)
{
    BG = bg;
}

void FB::update_dirty_rect()
{
	dirty_start_y = caret_y < dirty_start_y ? caret_y : dirty_start_y;
	dirty_end_y = caret_y + 1 > dirty_end_y ? caret_y + 1 : dirty_end_y;
	dirty_start_x = caret_x < dirty_start_x ? caret_x : dirty_start_x;
	dirty_end_x = caret_x + 1 > dirty_end_x ? caret_x + 1 : dirty_end_x;
}

[[noreturn]] void FB::refresh_loop()
{
	static const uint frame_duration = 1000 / fps;
	while (true)
	{
		PIT::sleep(frame_duration);
		flush();
	}
}

void FB::flush()
{
	if (lock_flush || dirty_start_y == (uint)-1)
		return;

	auto cursor_shadow_off = shadow_start + caret_y * fb_width + caret_x;
	if (cursor_shadow_off >= shadow_lim)
		cursor_shadow_off -= shadow_lim;

	// Draw cursor
	fb_shadow[cursor_shadow_off] = {FG, BG, '_'};

	// Compute fb base offsets
	auto fb_y_off = dirty_start_y * font->characterSize * n_row;
	auto fb_x_off = dirty_start_x * 8;
	auto fb_start = fb + fb_y_off + fb_x_off;

	// Compute shadow base offsets
	auto shadow_y_off = dirty_start_y * fb_width + shadow_start;
	auto shadow_off = shadow_y_off + dirty_start_x;

	// Compute dirty rectangle dimensions
	auto n_y = dirty_end_y - dirty_start_y;
	auto n_x = dirty_end_x - dirty_start_x;

	for (uint y = 0; y < n_y; y++)
	{
		// Compute shadow row offset
		auto shadow_y_off2 = shadow_off + y * fb_width;
		if (shadow_y_off2 >= shadow_lim)
			shadow_y_off2 -= shadow_lim;
		// Compute fb row offset
		auto by = y * font->characterSize;

		for (uint x = 0; x < n_x; x++)
		{
			// Get cell info
			auto cell = fb_shadow[shadow_y_off2 + x];
			char c = cell.c;
			auto fg = cell.fb;
			auto bg = cell.bg;
			auto glyph = &r_font[c * font->characterSize * 8];

			// Compute column offset
			auto x8 = x * 8;

			// Draw character
			for (uint px_y = 0; px_y < font->characterSize; px_y++)
			{
				auto bytes = glyph + px_y * 8; // Get glyph row
				auto fb_dst = &fb_start[(by + px_y) * n_row + x8]; // Get fb row

				// Write pixels
				fb_dst[0] = bytes[0] ? fg : bg;
				fb_dst[1] = bytes[1] ? fg : bg;
				fb_dst[2] = bytes[2] ? fg : bg;
				fb_dst[3] = bytes[3] ? fg : bg;
				fb_dst[4] = bytes[4] ? fg : bg;
				fb_dst[5] = bytes[5] ? fg : bg;
				fb_dst[6] = bytes[6] ? fg : bg;
				fb_dst[7] = bytes[7] ? fg : bg;
			}
		}
	}

	 // Erase cursor from shadow as it is temporary
	fb_shadow[cursor_shadow_off] = {FG, BG, '\0'};

	// Reset dirty rectangle dimensions
	dirty_start_x = (uint)-1;
	dirty_start_y = (uint)-1;
	dirty_end_y = 0;
	dirty_end_x = 0;

	// Draw progress bar if needed
	if (progress_bar_owner)
	{
		// Draw bar
		uint n_full = (uint)((float)progress_bar_percentage * (float)fb_width / 100.f) * progress_bar_height;
		for (uint i = 0; i < n_full; i++)
			fb[i] = FG;
		// Clear the rest ot the line
		for (uint i = n_full; i < progress_bar_height * fb_width; i++)
			fb[i] = BG;
	}
}

void FB::init(uint fps)
{
	auto fb_tag = (multiboot_tag_framebuffer*)Multiboot::get_tag(MULTIBOOT_FRAMEBUFFER_TAG);

	if (!fb_tag)
		irrecoverable_error("Cannot find framebuffer multiboot tag");
	if (fb_tag->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_RGB)
		irrecoverable_error("Framebuffer is not in RGB mode");
	/*if (fb_tag->framebuffer_type != MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT)
		irrecoverable_error("Framebuffer is not in EGA text mode");*/
	if (fb_tag->framebuffer_bpp != 32)
		irrecoverable_error("Framebuffer has bpp != 32");

	auto rgb_tag = (multiboot_tag_framebuffer_rgb*)(fb_tag + 1);

	if (rgb_tag->framebuffer_red_field_position != 16 || rgb_tag->framebuffer_red_mask_size != 8 ||
	    rgb_tag->framebuffer_green_field_position != 8 || rgb_tag->framebuffer_green_mask_size != 8 ||
	    rgb_tag->framebuffer_blue_field_position != 0 || rgb_tag->framebuffer_blue_mask_size != 8)
		irrecoverable_error("Unsupported RGB layout");

	fb_width = fb_tag->framebuffer_width;
	fb_height = fb_tag->framebuffer_height;
	fb_pitch = fb_tag->framebuffer_pitch;
	n_row = fb_pitch / sizeof(uint32_t);

	auto fb_size = fb_pitch * fb_height;
	if (!((fb = (uint32_t*)Memory::register_physical_data(fb_tag->framebuffer_addr, fb_size))))
		irrecoverable_error("Cannot map framebuffer");

	fb_shadow = new cell[fb_width * fb_height];
	progress_bar_height = font->characterSize / 2;
	characters_per_line = fb_width / 8;
	characters_per_col = fb_height / font->characterSize;
	shadow_lim = fb_width * fb_height;
	FB::fps = fps;

	FG = WHITE;
	BG = BLACK;

	// Initialize rasterized font
	r_font = new uint32_t[font->characterSize * 8 * 255];
	for (uint i = 0; i < 255; i++)
	{
		auto glyph = &glyphs[i * font->characterSize];
		auto i_off = i * font->characterSize * 8;
		for (uint y = 0; y < font->characterSize; y++)
		{
			auto line = glyph[y];
			auto dest = &r_font[i_off + y * 8];
			dest[0] = line & 0x80 ? FG : BG;
			dest[1] = line & 0x40 ? FG : BG;
			dest[2] = line & 0x20 ? FG : BG;
			dest[3] = line & 0x10 ? FG : BG;
			dest[4] = line & 0x08 ? FG : BG;
			dest[5] = line & 0x04 ? FG : BG;
			dest[6] = line & 0x02 ? FG : BG;
			dest[7] = line & 0x01 ? FG : BG;
		}
	}

	// Make character 0 representation to be empty
	memset(r_font, 0, font->characterSize * 8 * sizeof(uint32_t));

    /*outb(FB_COMMAND_PORT, CURSOR_END_LINE); // set the cursor end line to 15
    outb(FB_DATA_PORT, 0x0F);

    outb(FB_COMMAND_PORT, CURSOR_BEGIN_LINE); // set the cursor start line to 14 and enable cursor visibility
    outb(FB_DATA_PORT, 0x0E);*/
    clear_screen();
}

char* k__int_str(intmax_t i, char b[], int base, uint plusSignIfNeeded, uint spaceSignIfNeeded,
                 int paddingNo, uint justify, uint zeroPad)
{
    char digit[32] = {0};
    memset(digit, 0, 32);
    strcpy(digit, "0123456789");

    if (base == 16)
    {
        strcat(digit, "ABCDEF");
    }
    else if (base == 17)
    {
        strcat(digit, "abcdef");
        base = 16;
    }

    char* p = b;
    if (i < 0)
    {
        *p++ = '-';
        i *= -1;
    }
    else if (plusSignIfNeeded)
    {
        *p++ = '+';
    }
    else if (!plusSignIfNeeded && spaceSignIfNeeded)
    {
        *p++ = ' ';
    }

    intmax_t shifter = i;
    do
    {
        ++p;
        shifter = shifter / base;
    }
    while (shifter);

    *p = '\0';
    do
    {
        *--p = digit[i % base];
        i = i / base;
    }
    while (i);

    int padding = paddingNo - (int)strlen(b);
    if (padding < 0) padding = 0;

    if (justify)
    {
        while (padding--)
        {
            if (zeroPad)
            {
                b[strlen(b)] = '0';
            }
            else
            {
                b[strlen(b)] = ' ';
            }
        }
    }
    else
    {
        char a[256] = {0};
        while (padding--)
        {
            if (zeroPad)
            {
                a[strlen(a)] = '0';
            }
            else
            {
                a[strlen(a)] = ' ';
            }
        }
        strcat(a, b);
        strcpy(b, a);
    }

    return b;
}

int kvprintf(const char* format, void (*output_char_func)(char, int*), void (*output_string_func)(const char*, int*), va_list list)
{
	int chars = 0;
	char intStrBuffer[256] = {0};

	for (int i = 0; format[i]; ++i)
	{

		char specifier = '\0';
		char length = '\0';

		int lengthSpec = 0;
		int precSpec = 0;
		uint leftJustify = 0;
		uint zeroPad = 0;
		uint spaceNoSign = 0;
		uint altForm = 0;
		uint plusSign = 0;
		uint emode = 0;
		int expo = 0;

		if (format[i] == '%')
		{
			++i;

			uint extBreak = 0;
			while (1)
			{

				switch (format[i])
				{
					case '-':
						leftJustify = 1;
						++i;
						break;

					case '+':
						plusSign = 1;
						++i;
						break;

					case '#':
						altForm = 1;
						++i;
						break;

					case ' ':
						spaceNoSign = 1;
						++i;
						break;

					case '0':
						zeroPad = 1;
						++i;
						break;

					default:
						extBreak = 1;
						break;
				}

				if (extBreak) break;
			}

			while (isdigit(format[i]))
			{
				lengthSpec *= 10;
				lengthSpec += format[i] - 48;
				++i;
			}

			if (format[i] == '*')
			{
				lengthSpec = va_arg(list, int);
				++i;
			}

			if (format[i] == '.')
			{
				++i;
				while (isdigit(format[i]))
				{
					precSpec *= 10;
					precSpec += format[i] - 48;
					++i;
				}

				if (format[i] == '*')
				{
					precSpec = va_arg(list, int);
					++i;
				}
			}
			else
			{
				precSpec = 6;
			}

			if (format[i] == 'h' || format[i] == 'l' || format[i] == 'j' ||
				format[i] == 'z' || format[i] == 't' || format[i] == 'L')
			{
				length = format[i];
				++i;
				if (format[i] == 'h')
				{
					length = 'H';
				}
				else if (format[i] == 'l')
				{
					length = 'q';
					++i;
				}
			}
			specifier = format[i];

			memset(intStrBuffer, 0, 256);

			int base = 10;
			if (specifier == 'o')
			{
				base = 8;
				specifier = 'u';
				if (altForm)
				{
					output_string_func("0", &chars);
				}
			}
			if (specifier == 'p')
			{
				base = 16;
				length = 'z';
				specifier = 'u';
			}
			switch (specifier)
			{
				case 'X':
					base = 16;
					// fallthrough
				case 'x':
					base = base == 10 ? 17 : base;
					if (altForm)
					{
						output_string_func("0x", &chars);
					}
					// fallthrough
				case 'u':
				{
					switch (length)
					{
						case 0:
						{
							uint integer = va_arg(list, uint);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'H':
						{
							unsigned char integer = (unsigned char) va_arg(list, uint);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'h':
						{
							unsigned short int integer = va_arg(list, uint);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'l':
						{
							unsigned long integer = va_arg(list, unsigned long);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'q':
						{
							unsigned long long integer = va_arg(list, unsigned long long);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'j':
						{
							uintmax_t integer = va_arg(list, uintmax_t);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'z':
						{
							size_t integer = va_arg(list, size_t);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 't':
						{
							ptrdiff_t integer = va_arg(list, ptrdiff_t);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						default:
							break;
					}
					break;
				}

				case 'd':
				case 'i':
				{
					switch (length)
					{
						case 0:
						{
							int integer = va_arg(list, int);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'H':
						{
							signed char integer = (signed char) va_arg(list, int);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'h':
						{
							short int integer = va_arg(list, int);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'l':
						{
							long integer = va_arg(list, long);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'q':
						{
							long long integer = va_arg(list, long long);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'j':
						{
							intmax_t integer = va_arg(list, intmax_t);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 'z':
						{
							size_t integer = va_arg(list, size_t);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						case 't':
						{
							ptrdiff_t integer = va_arg(list, ptrdiff_t);
							k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
									  zeroPad);
							output_string_func(intStrBuffer, &chars);
							break;
						}
						default:
							break;
					}
					break;
				}

				case 'c':
				{
					if (length == 'l')
					{
						output_char_func(va_arg(list, wint_t), &chars);
					}
					else
					{
						output_char_func(va_arg(list, int), &chars);
					}

					break;
				}

				case 's':
				{
					output_string_func(va_arg(list, char*), &chars);
					break;
				}

				case 'n':
				{
					switch (length)
					{
						case 'H':
							*(va_arg(list, signed char*)) = chars;
							break;
						case 'h':
							*(va_arg(list, short int*)) = chars;
							break;

						case 0:
						{
							int* a = va_arg(list, int*);
							*a = chars;
							break;
						}

						case 'l':
							*(va_arg(list, long*)) = chars;
							break;
						case 'q':
							*(va_arg(list, long long*)) = chars;
							break;
						case 'j':
							*(va_arg(list, intmax_t*)) = chars;
							break;
						case 'z':
							*(va_arg(list, size_t*)) = chars;
							break;
						case 't':
							*(va_arg(list, ptrdiff_t*)) = chars;
							break;
						default:
							break;
					}
					break;
				}

				case 'e':
				case 'E':
					emode = 1;
					// fallthrough

				case 'f':
				case 'F':
				case 'g':
				case 'G':
				{
					double floating = va_arg(list, double);

					while (emode && floating >= 10)
					{
						floating /= 10;
						++expo;
					}

					int form = lengthSpec - precSpec - expo - (precSpec || altForm ? 1 : 0);
					if (emode)
					{
						form -= 4;      // 'e+00'
					}
					if (form < 0)
					{
						form = 0;
					}

					k__int_str(floating, intStrBuffer, base, plusSign, spaceNoSign, form, \
                              leftJustify, zeroPad);

					output_string_func(intStrBuffer, &chars);

					floating -= (int) floating;

					for (int i = 0; i < precSpec; ++i)
					{
						floating *= 10;
					}
					intmax_t decPlaces = (intmax_t) (floating + 0.5);

					if (precSpec)
					{
						output_char_func('.', &chars);
						k__int_str(decPlaces, intStrBuffer, 10, 0, 0, 0, 0, 0);
						intStrBuffer[precSpec] = 0;
						output_string_func(intStrBuffer, &chars);
					}
					else if (altForm)
					{
						output_char_func('.', &chars);
					}

					break;
				}


				case 'a':
				case 'A':
					//ACK! Hexadecimal floating points...
					break;

				default:
					break;
			}

			if (specifier == 'e')
			{
				output_string_func("e+", &chars);
			}
			else if (specifier == 'E')
			{
				output_string_func("E+", &chars);
			}

			if (specifier == 'e' || specifier == 'E')
			{
				k__int_str(expo, intStrBuffer, 10, 0, 0, 2, 0, 1);
				output_string_func(intStrBuffer, &chars);
			}

		}
		else
		{
			output_char_func(format[i], &chars);
		}
	}

	return chars;
}

#define string_method_of_char_method_name(method) method##_string

#define string_version_of_char_method(method_name) \
	void string_method_of_char_method_name(method_name)(const char* c, int* a) \
	{ \
		for (int i = 0; c[i]; ++i) \
		{ \
			method_name(c[i], a); \
		} \
	}

void kprintf_output(char c, int* a)
{
	FB::putchar(c);
	*a += 1;
}

string_version_of_char_method(kprintf_output)

static char* ksprintf_buf = nullptr;
void ksprintf_output(char c, int* a)
{
  	*ksprintf_buf++ = c;
    *a += 1;
}

string_version_of_char_method(ksprintf_output)

#define output_funcs(method_name) method_name, string_method_of_char_method_name(method_name)

__attribute__ ((format (printf, 1, 2))) int printf(const char* format, ...)
{
	va_list list;
	va_start (list, format);
	int i = kvprintf(format, output_funcs(kprintf_output), list);
	va_end (list);
	return i;
}

__attribute__((format(printf, 2, 3))) int sprintf(char* str, const char *format, ...)
{
  	ksprintf_buf = str;
	va_list list;
	va_start (list, format);
	int i = kvprintf(format, output_funcs(ksprintf_output), list);
	va_end (list);
	ksprintf_buf = nullptr;
	return i;
}

#define MSG_PRINTF(type, format) { \
    FB::type##_decorator(); \
    FB::putchar(' '); \
    va_list list; \
    va_start(list, format); \
    int i = kvprintf(format, output_funcs(kprintf_output), list); \
    va_end(list); \
    FB::putchar(' '); \
    FB::type(); \
    return i; \
}

__attribute__ ((format (printf, 1, 2))) int printf_error(const char* format, ...)
{
    MSG_PRINTF(error, format)
}

__attribute__ ((format (printf, 1, 2))) int printf_info(const char* format, ...)
{
    MSG_PRINTF(info, format)
}

int printf_warn(const char* format, ...)
{
    MSG_PRINTF(warn, format)
}

[[noreturn]]
__attribute__ ((format (printf, 1, 2))) int irrecoverable_error(const char* format, ...)
{
	auto fmt_len = strlen(format);
	const char* prelude = "IRRECOVERABLE ERROR: ";
	char*full_format = new char[fmt_len + strlen(prelude) + 1];
	strcpy(full_format, prelude);
	strcat(full_format, format);

	FB::error_decorator();
	FB::putchar(' ');
	va_list list;
	va_start(list, format);
	kvprintf(full_format, output_funcs(kprintf_output), list);
	va_end(list);
	FB::putchar(' ');
	FB::error();

	System::shutdown();
}
