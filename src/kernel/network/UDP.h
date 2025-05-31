#ifndef UDP_H
#define UDP_H

#include <stdint.h>
#include <kstddef.h>

#include "Ethernet.h"
#include "IPV4.h"
#include "NetworkConsts.h"

class UDP
{
public:
    struct header
    {
        uint16_t src_port;
        uint16_t dst_port;
        uint16_t length;
        uint16_t checksum;
    };

    typedef struct header header_t;

    struct packet
    {
        header_t header;
        uint8_t payload[];
    };

    typedef struct packet packet_t;

    struct packet_info
    {
        packet_t* packet;
        size_t size;

        packet_info(void* ptr, uint16_t size)
            : packet((packet_t*)ptr),
              size(size)
        {
        }
    };

    typedef struct packet_info packet_info_t;

    static uint8_t* create_packet(uint16_t src_port, uint16_t dst_port, uint16_t payload_size, uint32_t daddr,
                                  uint8_t dst_mac[MAC_ADDR_LEN], Ethernet::packet_info_t& ethernet_packet_info);

    static void handle_packet(const packet_info_t* packet_info, const IPV4::packet_t* ipv4_packet,
                              const Ethernet::packet_t* ethernet_packet);

    [[nodiscard]] static size_t get_header_size();
private:

    static uint16_t write_header(uint8_t* buf, uint16_t src_port, uint16_t dst_port, uint16_t payload_size);

    static bool packet_valid(const packet_info_t* packet_info);
};


#endif //UDP_H
