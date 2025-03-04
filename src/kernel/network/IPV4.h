#ifndef IPV4_H
#define IPV4_H

// https://en.wikipedia.org/wiki/IPv4#Header

#include <kstdint.h>
#include <kstddef.h>

#include "Ethernet.h"

#define IPV4_FLAG_DONT_FRAGMENT 2
#define IPV4_FLAG_MORE_FRAGMENTS 4

#define IPV4_PROTOCOL_ICMP 1
#define IPV4_PROTOCOL_TCP 6
#define IPV4_PROTOCOL_UDP 17

#define IPV4_DEFAULT_TTL 64
#define IPV4_DEFAULT_IHL 5

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
        [[nodiscard]] uint8_t get_flags() const { return (flags_and_frag_offset >> 5) & 0x7; }
        [[nodiscard]] uint16_t get_frag_offset() const { return flags_and_frag_offset & 0xFF1F; }

        void set_version(uint8_t v) { version_and_ihl |= (v & 0xF) << 4; }
        void set_ihl(uint8_t v) { version_and_ihl |= v & 0xF; }
        void set_flags(uint8_t v) { flags_and_frag_offset |= (v & 0x7) << 5; }
        void set_frag_offset_to_zero() { flags_and_frag_offset &= ~0xFF1F; }
    } __attribute__((packed));

    typedef struct header header_t;

    struct packet
    {
        header_t header;
        uint8_t payload[];
    };

    typedef struct packet packet_t;

    static void handlePacket(const packet_t* packet, uint8_t* response_buffer);

    [[nodiscard]] static size_t get_response_size(const packet_t* packet_info);

    [[nodiscard]] static size_t get_headers_size();

    [[nodiscard]] static size_t get_header_size();

    static void write_response(uint8_t* response_buf, const header_t* request_header, size_t payload_size);

    static void write_packet(uint8_t* buf, uint8_t tos, uint16_t size, uint16_t id, uint8_t flags, uint8_t ttl,
                             uint8_t proto, uint32_t daddr);

    static uint8_t* write_headers(uint8_t* buf, uint16_t payload_size, uint8_t proto, uint32_t daddr,
                                  uint8_t dst_mac[MAC_ADDR_LEN]);

    [[nodiscard]] static bool address_is_in_subnet(const uint8_t address[IPV4_ADDR_LEN]);
};


#endif //IPV4_H
