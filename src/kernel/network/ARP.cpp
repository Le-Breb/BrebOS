#include "ARP.h"

#include "Network.h"
#include "../core/fb.h"
#include "../core/memory.h"

Ethernet::packet_info* ARP::new_broadcast_request(uint8_t srchw[ARP_MAC_ADDR_LEN])
{
    uint16_t htype = ARP_ETHERNET;
    uint16_t ptype = ARP_IPV4;
    uint8_t hlen = ARP_MAC_ADDR_LEN;
    uint8_t plen = ARP_IPV4_ADDR_LEN;
    uint16_t opcode = ARP_REQUEST;
    uint8_t dsthw[ARP_MAC_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

    auto buf = (uint8_t*)calloc(1, sizeof(Ethernet::header_t) + sizeof(packet_t));
    write_packet((packet_t*)(buf + sizeof(Ethernet::header_t)), htype, ptype, hlen, plen, opcode, srchw,
                 *(uint32_t*)Network::ip, dsthw, 0);
    Ethernet::write_header((Ethernet::header*)buf, (uint8_t*)Network::broadcast_mac, Network::mac, ETHERTYPE_ARP);

    return new Ethernet::packet_info((Ethernet::packet_t*)buf, sizeof(Ethernet::header) + sizeof(packet));
}

Ethernet::packet_info* ARP::new_reply(packet_t* request)
{
    uint16_t htype = ARP_ETHERNET;
    uint16_t ptype = ARP_IPV4;
    uint8_t hlen = ARP_MAC_ADDR_LEN;
    uint8_t plen = ARP_IPV4_ADDR_LEN;
    uint16_t opcode = ARP_REPLY;

    auto buf = (uint8_t*)calloc(1, sizeof(Ethernet::header_t) + sizeof(packet_t));
    write_packet((packet_t*)(buf + sizeof(Ethernet::header_t)), htype, ptype, hlen, plen, opcode, Network::mac,
                 *(uint32_t*)&Network::ip, request->srchw, request->srcpr);
    Ethernet::write_header((Ethernet::header*)buf, (uint8_t*)Network::broadcast_mac, Network::mac, ETHERTYPE_ARP);

    return new Ethernet::packet_info((Ethernet::packet_t*)buf, sizeof(Ethernet::header) + sizeof(packet));
}

void ARP::display_request(packet_t* p)
{
    uint32_t rev_srcpr = Endianness::switch_endian32(p->srcpr);
    uint32_t rev_dstpr = Endianness::switch_endian32(p->dstpr);
    printf(
        "Hardware Type: 0x%04x, Protocol Type: 0x%04x, HW Len: %d, Protocol Len: %d, Opcode: %s, Src MAC: %02x:%02x:%02x:%02x:%02x:%02x, Src IP: %d.%d.%d.%d, Dst MAC: %02x:%02x:%02x:%02x:%02x:%02x, Dst IP: %d.%d.%d.%d\n",
        p->htype, p->ptype, p->hlen, p->plen, p->opcode == 1 ? "ARP Request" : "ARP Reply",
        p->srchw[0], p->srchw[1], p->srchw[2], p->srchw[3], p->srchw[4], p->srchw[5],
        (rev_srcpr >> 24) & 0xFF, (rev_srcpr >> 16) & 0xFF, (rev_srcpr >> 8) & 0xFF, rev_srcpr & 0xFF,
        p->dsthw[0], p->dsthw[1], p->dsthw[2], p->dsthw[3], p->dsthw[4], p->dsthw[5],
        (rev_dstpr >> 24) & 0xFF, (rev_dstpr >> 16) & 0xFF, (rev_dstpr >> 8) & 0xFF, rev_dstpr & 0xFF);
}
