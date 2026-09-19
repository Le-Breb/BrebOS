#ifndef INCLUDE_INTERRUPT_NUMBERS_H
#define INCLUDE_INTERRUPT_NUMBERS_H

namespace InterruptNumber
{
    constexpr int DEBUG          = 0x01;
    constexpr int INVALID_OPCODE = 0x06;
    constexpr int GPF            = 0x0D;
    constexpr int PAGE_FAULT     = 0x0E;
    constexpr int TIMER          = 0x20;
    constexpr int KEYBOARD       = 0x21;
    constexpr int SYSCALL        = 0x80;
}

#endif //INCLUDE_INTERRUPT_NUMBERS_H
