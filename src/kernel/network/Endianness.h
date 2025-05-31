#ifndef ENDIANNESS_H
#define ENDIANNESS_H
#include <stdint.h>


class Endianness {
public:
    static uint32_t switch32(uint32_t nb) {
        return ((nb>>24)&0xff)      |
               ((nb<<8)&0xff0000)   |
               ((nb>>8)&0xff00)     |
               ((nb<<24)&0xff000000);
    }

    static uint16_t switch16(uint16_t nb)
    {
        return ((nb>>8)&0xff) | ((nb<<8)&0xff00);
    }
};



#endif //ENDIANNESS_H
