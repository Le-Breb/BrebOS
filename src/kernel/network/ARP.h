#ifndef ARP_H
#define ARP_H

#include <stdint.h>
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

    static Ethernet::packet_info_t pending_queue[ARP_PENDING_QUEUE_SIZE]; // Queue of packets waiting for destination MAC to be resolved
    static size_t pending_queue_head;

    static cache_entry_t cache[ARP_CACHE_SIZE]; // IP-MAC cache
    static size_t cache_head;

    /**
     * sends ARP request to get MAC of given IP
     * @param ip ip to be resolved
     */
    static void resolve_mac(const uint8_t ip[IPV4_ADDR_LEN]);

    [[nodiscard]] static bool is_valid(const packet_t* packet);

    [[nodiscard]] static uint16_t get_header_size();
public:
    static void resolve_gateway_mac();

    static void write_packet(uint8_t* buf, uint16_t opcode, uint32_t dstpr, uint8_t dsthw[MAC_ADDR_LEN]);

    static void display_request(packet_t *p);

    static void handle_packet(const packet_t* packet, const Ethernet::packet_t* ethernet_packet);

    /**
     * Put packet in waiting queue while sending a request to get its destination MAC. Packet will be be sent upon
     * reception of ARP response
     * @param packet_info packet to be sent
     */
    static void resolve_and_send(const Ethernet::packet_info_t* packet_info);

    /**
     * Retrieves cached MAC address associated with given IP
     * @param ip ip to be resolved
     * @param mac buffer where resolved mac will be written to. Is zeroed out if MAC is not found in cache
     * @warning This does not send ARP requests if the MAC address is not found in the cache. See resolve_mac
     */
    static void get_mac(const uint8_t ip[IPV4_ADDR_LEN], uint8_t mac[MAC_ADDR_LEN]);
};



#endif //ARP_H
