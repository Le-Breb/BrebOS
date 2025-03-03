#ifndef ARP_H
#define ARP_H

#include <kstdint.h>
#include <kstring.h>

#include "Endianness.h"
#include "Ethernet.h"

#define ARP_ETHERNET 1
#define ARP_IPV4 0x800

#define ARP_REQUEST 1
#define ARP_REPLY 2

#define ARP_PENDING_QUEUE_SIZE 10
#define ARP_CACHE_SIZE 10

class ARP {
    public:
    struct packet
    {
        uint16_t htype; // Hardware type
        uint16_t ptype; // Protocol type
        uint8_t  hlen; // Hardware address length (Ethernet = 6)
        uint8_t  plen; // Protocol address length (IPv4 = 4)
        uint16_t opcode; // ARP Operation Code
        uint8_t  srchw[MAC_ADDR_LEN]{}; // Source hardware address - hlen bytes (see above)
        uint32_t srcpr; // Source protocol address - plen bytes (see above). If IPv4 can just be a "u32" type.
        uint8_t  dsthw[MAC_ADDR_LEN]{}; // Destination hardware address - hlen bytes (see above)
        uint32_t dstpr; // Destination protocol address - plen bytes (see above). If IPv4 can just be a "u32" type.
    } __attribute__((packed));

    typedef struct packet packet_t;
private:
    struct cache_entry
    {
        uint8_t ip[IPV4_ADDR_LEN]{};
        uint8_t mac[MAC_ADDR_LEN]{};
    };

    typedef struct cache_entry cache_entry_t;

    static Ethernet::packet_info_t pending_queue[ARP_PENDING_QUEUE_SIZE];
    static size_t pending_queue_head;
    static cache_entry_t cache[ARP_CACHE_SIZE];
    static size_t cache_head;
    static void resolve_mac(const uint8_t ip[IPV4_ADDR_LEN]);
public:
    static void resolve_gateway_mac();

    static void write_packet(uint8_t* buf, uint16_t htype, uint16_t ptype, uint8_t hlen, uint8_t plen, uint16_t opcode,
                             uint32_t srcpr, uint32_t dstpr, uint8_t dsthw[]);

    static Ethernet::packet_info* new_reply(packet_t* request);

    static void display_request(packet_t *p);

    static void handlePacket(packet_t* packet, uint8_t* response_buf);

    static void resolve_and_send(const Ethernet::packet_info_t* packet_info);

    [[nodiscard]] static size_t get_response_size(const packet_t* packet);

    [[nodiscard]] static size_t get_headers_size();

    static void get_mac(const uint8_t ip[IPV4_ADDR_LEN], uint8_t mac[MAC_ADDR_LEN]);
};



#endif //ARP_H
