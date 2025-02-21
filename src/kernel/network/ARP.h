#ifndef ARP_H
#define ARP_H

#include <kstdint.h>
#include <kstring.h>

#include "Endianness.h"
#include "Ethernet.h"

#define ARP_ETHERNET 1
#define ARP_IPV4 0x800
#define ARP_MAC_ADDR_LEN 6
#define ARP_IPV4_ADDR_LEN 4


#define ARP_REQUEST 1
#define ARP_REPLY 2


class ARP {
    public:
    struct packet
    {
        uint16_t htype; // Hardware type
        uint16_t ptype; // Protocol type
        uint8_t  hlen; // Hardware address length (Ethernet = 6)
        uint8_t  plen; // Protocol address length (IPv4 = 4)
        uint16_t opcode; // ARP Operation Code
        uint8_t  srchw[ARP_MAC_ADDR_LEN]{}; // Source hardware address - hlen bytes (see above)
        uint32_t srcpr; // Source protocol address - plen bytes (see above). If IPv4 can just be a "u32" type.
        uint8_t  dsthw[ARP_MAC_ADDR_LEN]{}; // Destination hardware address - hlen bytes (see above)
        uint32_t dstpr; // Destination protocol address - plen bytes (see above). If IPv4 can just be a "u32" type.
    } __attribute__((packed));

    typedef struct packet packet_t;

    static void write_packet(packet_t* ptr, uint16_t htype, uint16_t ptype, uint8_t hlen, uint8_t plen, uint16_t opcode, uint8_t srchw[ARP_MAC_ADDR_LEN],
        uint32_t srcpr, uint8_t dsthw[ARP_MAC_ADDR_LEN], uint32_t dstpr)
    {
        ptr->htype = Endianness::switch_endian16(htype);
        ptr->ptype = Endianness::switch_endian16(ptype);
        ptr->hlen = hlen;
        ptr->plen = plen;
        ptr->opcode = Endianness::switch_endian16(opcode);
        ptr->htype = Endianness::switch_endian16(htype);
        ptr->hlen = hlen;
        ptr->plen = plen;
        ptr->srcpr = srcpr;
        ptr->dstpr = dstpr;
        memcpy(ptr->srchw, srchw, ARP_MAC_ADDR_LEN);
        memcpy(ptr->dsthw, dsthw, ARP_MAC_ADDR_LEN);
    }

    static Ethernet::packet_info* new_broadcast_request(uint8_t srchw[ARP_MAC_ADDR_LEN]);

    static Ethernet::packet_info* new_reply(packet_t* request);

    static void display_request(packet_t *p);
};



#endif //ARP_H
