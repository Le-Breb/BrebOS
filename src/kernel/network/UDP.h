#ifndef UDP_H
#define UDP_H

#include <kstdint.h>
#include <kstddef.h>

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
        uint8_t data[];
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

    [[nodiscard]] static size_t get_headers_size();

    [[nodiscard]] static size_t get_header_size();

    static void write_header(uint8_t* buf, uint16_t src_port, uint16_t dst_port, uint16_t payload_size);

    static uint8_t* write_headers(uint8_t* buf, uint16_t src_port, uint16_t dst_port, uint16_t payload_size,
                           uint32_t daddr, uint8_t dst_mac[MAC_ADDR_LEN]);

    static void handlePacket(const packet_info_t* packet_info, uint8_t* response_buffer);

    static size_t get_response_size(const packet_info_t* packet_info);
};


#endif //UDP_H
