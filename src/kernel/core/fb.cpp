#include "fb.h"

#include <kstdio.h>
#include <kctype.h>
#include <kwchar.h>
#include <kstring.h>

#include "IO.h"

uint FB::caret_pos = 0;
short* const FB::fb = (short*)FB_ADDR;
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
    fb[caret_pos++] = (short)((((BG & 0x0F) << 4) | (FG & 0x0F)) << 8 | c);
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

void kdisplayCharacter(char c, int* a)
{
    FB::putchar(c);
    *a += 1;
}

void kdisplayString(const char* c, int* a)
{
    for (int i = 0; c[i]; ++i)
    {
        kdisplayCharacter(c[i], a);
    }
}

int kvprintf(const char* format, va_list list)
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
                    kdisplayString("0", &chars);
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
                    kdisplayString("0x", &chars);
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
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'H':
                        {
                            unsigned char integer = (unsigned char)va_arg(list, uint);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'h':
                        {
                            unsigned short int integer = va_arg(list, uint);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'l':
                        {
                            unsigned long integer = va_arg(list, unsigned long);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'q':
                        {
                            unsigned long long integer = va_arg(list, unsigned long long);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'j':
                        {
                            uintmax_t integer = va_arg(list, uintmax_t);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'z':
                        {
                            size_t integer = va_arg(list, size_t);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 't':
                        {
                            ptrdiff_t integer = va_arg(list, ptrdiff_t);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
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
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'H':
                        {
                            signed char integer = (signed char)va_arg(list, int);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'h':
                        {
                            short int integer = va_arg(list, int);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'l':
                        {
                            long integer = va_arg(list, long);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'q':
                        {
                            long long integer = va_arg(list, long long);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'j':
                        {
                            intmax_t integer = va_arg(list, intmax_t);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 'z':
                        {
                            size_t integer = va_arg(list, size_t);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
                            break;
                        }
                    case 't':
                        {
                            ptrdiff_t integer = va_arg(list, ptrdiff_t);
                            k__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify,
                                       zeroPad);
                            kdisplayString(intStrBuffer, &chars);
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
                        kdisplayCharacter(va_arg(list, wint_t), &chars);
                    }
                    else
                    {
                        kdisplayCharacter(va_arg(list, int), &chars);
                    }

                    break;
                }

            case 's':
                {
                    kdisplayString(va_arg(list, char*), &chars);
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
                        form -= 4; // 'e+00'
                    }
                    if (form < 0)
                    {
                        form = 0;
                    }

                    k__int_str(floating, intStrBuffer, base, plusSign, spaceNoSign, form,
                               leftJustify, zeroPad);

                    kdisplayString(intStrBuffer, &chars);

                    floating -= (int)floating;

                    for (int i = 0; i < precSpec; ++i)
                    {
                        floating *= 10;
                    }
                    intmax_t decPlaces = (intmax_t)(floating + 0.5);

                    if (precSpec)
                    {
                        kdisplayCharacter('.', &chars);
                        k__int_str(decPlaces, intStrBuffer, 10, 0, 0, 0, 0, 0);
                        intStrBuffer[precSpec] = 0;
                        kdisplayString(intStrBuffer, &chars);
                    }
                    else if (altForm)
                    {
                        kdisplayCharacter('.', &chars);
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
                kdisplayString("e+", &chars);
            }
            else if (specifier == 'E')
            {
                kdisplayString("E+", &chars);
            }

            if (specifier == 'e' || specifier == 'E')
            {
                k__int_str(expo, intStrBuffer, 10, 0, 0, 2, 0, 1);
                kdisplayString(intStrBuffer, &chars);
            }
        }
        else
        {
            kdisplayCharacter(format[i], &chars);
        }
    }

    return chars;
}

__attribute__ ((format (printf, 1, 2))) int printf([[maybe_unused]] const char* format, ...)
{
    va_list list;
    va_start(list, format);
    int i = kvprintf(format, list);
    va_end(list);
    return i;
}

__attribute__ ((format (printf, 1, 2))) int printf_error(const char* format, ...)
{
    FB::error_decorator();
    FB::putchar(' ');
    va_list list;
    va_start(list, format);
    int i = kvprintf(format, list);
    va_end(list);
    FB::putchar(' ');
    FB::error();

    return i;
}

__attribute__ ((format (printf, 1, 2))) int printf_info(const char* format, ...)
{
    FB::info_decorator();
    FB::putchar(' ');
    va_list list;
    va_start(list, format);
    int i = kvprintf(format, list);
    va_end(list);
    FB::putchar(' ');
    FB::info();

    return i;
}
