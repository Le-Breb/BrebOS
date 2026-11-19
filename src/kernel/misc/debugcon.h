#pragma once

#include <stdint.h>

// QEMU debug console: bytes written to port 0xE9 are forwarded to the -debugcon chardev.
// Without -debugcon, the port is unassigned and writes are ignored.

inline void debugcon_putc(char c)
{
    asm volatile ("outb %0, %1"
                  :
                  : "a"(c), "Nd"((uint16_t)0xE9));
}

inline void debugcon_puts(const char* string)
{
    for (; *string; debugcon_putc(*string++)) {}
}
