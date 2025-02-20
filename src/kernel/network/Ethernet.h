#ifndef ETHERNET_H
#define ETHERNET_H
#include <kstdint.h>

#define ETHERTYPE_ARP 0x806

class Ethernet
{
public:
    struct packet
    {
        uint8_t dest[6];
        uint8_t src[6];
        uint16_t type;
    };
};


#endif //ETHERNET_H
