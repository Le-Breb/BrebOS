#ifndef ETHERNET_H
#define ETHERNET_H
#include <kstdint.h>
#include <kstring.h>

#define ETHERNET_MAC_LEN 6
#define ETHERNET_IPV3_LEN 4
#define ETHERTYPE_ARP 0x806

class Ethernet
{
public:
    struct header
    {
        uint8_t dest[ETHERNET_MAC_LEN]{};
        uint8_t src[ETHERNET_MAC_LEN]{};
        uint16_t type;
    };

    typedef struct header header_t;

    struct packet
    {
        header_t* header;
        uint8_t* payload;
    };

    typedef struct packet packet_t;

    struct packet_info
    {
        packet_t* packet;
        uint8_t size;

        packet_info(packet_t* packet, uint8_t size)
            : packet(packet),
              size(size)
        {
        }
    };

    static void write_header(header_t* ptr, uint8_t dest[ETHERNET_MAC_LEN], uint8_t src[ETHERNET_MAC_LEN], uint16_t type);
};


#endif //ETHERNET_H
