#include "keyboard.h"

#include "fb.h"
#include "interrupts.h"
#include "../processes/scheduler.h"

#define CTRL_PRESSED 1
#define CTRL_RELEASED 157

#define SHIFT_PRESSED 2
#define LEFT_SHIFT_RELEASED 170
#define RIGHT_SHIFT_RELEASED 182

bool ctrl_hold = false;
bool shift_hold = false;

char Keyboard::kbd_US[128] =
{
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', /* <-- Tab */
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    CTRL_PRESSED, /* <-- control key */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', SHIFT_PRESSED /* Left shift */, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm',
    ',', '.', '/', SHIFT_PRESSED /* Right shift */,
    '*',
    0, /* Alt */
    ' ', /* Space bar */
    0, /* Caps lock */
    0, /* 59 - F1 key ... > */
    0, 0, 0, 0, 0, 0, 0, 0,
    0, /* < ... F10 */
    0, /* 69 - Num lock*/
    0, /* Scroll Lock */
    0, /* Home key */
    0, /* Up Arrow */
    0, /* Page Up */
    '-',
    0, /* Left Arrow */
    0,
    0, /* Right Arrow */
    '+',
    0, /* 79 - End key*/
    0, /* Down Arrow */
    0, /* Page Down */
    0, /* Insert Key */
    0, /* Delete Key */
    0, 0, 0,
    0, /* F11 Key */
    0, /* F12 Key */
    0, /* All other keys are undefined */
};

unsigned char Keyboard::read_scan_code()
{
    return inb(KBD_DATA_PORT);
}

void Keyboard::handle_char(unsigned char c)
{
    //printf("%d\n", c);
    if (c < 128)
    {
        c = kbd_US[c];
        if (c == CTRL_PRESSED)
        {
            ctrl_hold = true;
            return;
        }
        if (c == SHIFT_PRESSED)
        {
            shift_hold = true;
            return;
        }
        // Select the correct character based on the shift key
        if (shift_hold)
        {
            switch (c)
            {
                case '1':
                    c = '!';
                    break;
                case '2':
                    c = '@';
                    break;
                case '3':
                    c = '#';
                    break;
                case '4':
                    c = '$';
                    break;
                case '5':
                    c = '%';
                    break;
                case '6':
                    c = '^';
                    break;
                case '7':
                    c = '&';
                    break;
                case '8':
                    c = '*';
                    break;
                case '9':
                    c = '(';
                    break;
                case '0':
                    c = ')';
                    break;
                case '-':
                    c = '_';
                    break;
                case '=':
                    c = '+';
                    break;
                case '[':
                    c = '{';
                    break;
                case ']':
                    c = '}';
                    break;
                case ';':
                    c = ':';
                    break;
                case '\'':
                    c = '"';
                    break;
                case '\\':
                    c = '|';
                    break;
                case '`':
                    c = '~';
                    break;
                case '/':
                    c = '?';
                    break;
                case ',':
                    c = '<';
                    break;
                case '.':
                    c = '>';
                    break;
                default:
                    break;
            }
        }

        if (c)
            Scheduler::wake_up_key_waiting_processes(c);
    }
    else if (c == CTRL_RELEASED)
        ctrl_hold = false;
    else if (c == LEFT_SHIFT_RELEASED || c == RIGHT_SHIFT_RELEASED)
        shift_hold = false;
}

void Keyboard::interrupt_handler()
{
    unsigned char scan_code = read_scan_code();

    handle_char(scan_code);
}
