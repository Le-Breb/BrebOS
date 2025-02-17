#ifndef PORTS_H_
#define PORTS_H_

#include <kstdint.h>


class Ports
{
    public:
        static void outportb (uint16_t p_port,uint8_t data);
        static void outportw (uint16_t p_port,uint16_t data);
        static void outportl (uint16_t p_port,uint32_t data);
        static uint8_t inportb( uint16_t p_port);
        static uint16_t inportw( uint16_t p_port);
        static uint32_t inportl( uint16_t p_port);
};

#endif /* PORTS_H_ */