#ifndef ICMP_H
#define ICMP_H

// https://en.wikipedia.org/wiki/Internet_Control_Message_Protocol

#include <kstdint.h>
#include <kstddef.h>

#include "IPV4.h"

#define ICMP_ECHO_REPLY 0
#define ICMP_ECHO_REQUEST 8

class ICMP
{
public:
    struct header
    {
        uint8_t type;
        uint8_t code; // subtype
        uint16_t csum;
    } __attribute__ ((packed));

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
        uint16_t size;

        packet_info(void* ptr, uint16_t size)
            : packet((struct packet*)ptr),
              size(size)
        {
        }
    };

    typedef struct packet_info packet_info_t;

    static void handle_packet(const packet_info_t* packet_info, const IPV4::packet_t* ipv4_packet, const Ethernet::packet_t* ethernet_packet);

private:

    static void write_ping_reply(uint8_t* response_buf, const packet_info_t* ping_request);

    [[nodiscard]] static bool packet_valid(const packet_info_t* packet_info);

    [[nodiscard]] static uint16_t get_packet_size();

    [[nodiscard]] static uint16_t get_response_size(const packet_info_t* request);
};


#endif //ICMP_H
