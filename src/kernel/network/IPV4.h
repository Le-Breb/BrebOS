#ifndef IPV4_H
#define IPV4_H

// https://en.wikipedia.org/wiki/IPv4#Header

#include <kstdint.h>
#include <kstddef.h>

#define IPV4_FLAG_DONT_FRAGMENT 1
#define IPV4_FLAG_MORE_FRAGMENTS 2

#define IPV4_PROTOCOL_ICMP 1

class IPV4
{
public:
    struct header
    {
        uint8_t version_and_ihl; // Internet Header Length (number of 32-bits words) and IP version
        uint8_t tos;
        uint16_t len;
        uint16_t id;
        uint16_t flags_and_frag_offset;
        uint8_t ttl;
        uint8_t proto;
        uint16_t csum;
        uint32_t saddr;
        uint32_t daddr;

        [[nodiscard]] uint8_t get_ihl() const { return version_and_ihl & 0xF; }
        [[nodiscard]] uint8_t get_version() const { return version_and_ihl >> 4; }
        [[nodiscard]] uint8_t get_flags() const { return flags_and_frag_offset & 0xE0; }
        [[nodiscard]] uint16_t get_frag_offset() const { return flags_and_frag_offset & 0xFF1F; } // Bit extraction is right, but bit ordering may be wrong
    } __attribute__((packed));

    typedef struct header header_t;

    static void handlePacket(header_t* header, uint8_t* response_buffer);

    static size_t get_response_size(const header_t* header);

    static void write_response(header_t* request_header, uint8_t* response_buffer);
};


#endif //IPV4_H
