#include "ARP.h"

#include "Network.h"
#include "../core/fb.h"
#include "../core/memory.h"

Ethernet::packet_info* ARP::new_arp_announcement(uint8_t srchw[MAC_ADDR_LEN])
{
    uint16_t htype = ARP_ETHERNET;
    uint16_t ptype = ARP_IPV4;
    uint8_t hlen = MAC_ADDR_LEN;
    uint8_t plen = IPV4_ADDR_LEN;
    uint16_t opcode = ARP_REQUEST;
    uint8_t dsthw[MAC_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

    auto buf = (uint8_t*)calloc(1, get_headers_size() + 0); // + 0 because we don't need a payload
    buf = Ethernet::write_header(buf, (uint8_t*)Network::broadcast_mac, Network::mac, ETHERTYPE_ARP);
    write_packet(buf + sizeof(Ethernet::header_t), htype, ptype, hlen, plen, opcode, srchw,
                 *(uint32_t*)Network::ip, dsthw, *(uint32_t*)Network::ip);

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

void ARP::handlePacket(packet_t* packet, uint8_t* response_buf)
{
    write_packet(response_buf, ARP_ETHERNET, ARP_IPV4, MAC_ADDR_LEN, IPV4_ADDR_LEN, ARP_REPLY, Network::mac,
                 *(uint32_t*)&Network::ip, packet->srchw, packet->srcpr);
}

size_t ARP::get_response_size(Ethernet::packet_info_t* packet_info)
{
    auto packet = (packet_t*)packet_info->packet->payload;
    if (!(0 == memcmp(&packet->dstpr, &Network::ip, sizeof(Network::ip)) || 0 == memcmp(
        &packet->dstpr, &Network::ip, sizeof(Network::broadcast_ip)))) // check that request is targeting us
        return -1;
    return sizeof(packet_t);
}

size_t ARP::get_headers_size()
{
    return sizeof(packet_t) + Ethernet::get_headers_size();
}

void ARP::write_packet(uint8_t* buf, uint16_t htype, uint16_t ptype, uint8_t hlen, uint8_t plen, uint16_t opcode,
                       uint8_t srchw[MAC_ADDR_LEN], uint32_t srcpr, uint8_t dsthw[MAC_ADDR_LEN], uint32_t dstpr)
{
    auto packet = (packet_t*)buf;
    packet->htype = Endianness::switch_endian16(htype);
    packet->ptype = Endianness::switch_endian16(ptype);
    packet->hlen = hlen;
    packet->plen = plen;
    packet->opcode = Endianness::switch_endian16(opcode);
    packet->htype = Endianness::switch_endian16(htype);
    packet->hlen = hlen;
    packet->plen = plen;
    packet->srcpr = srcpr;
    packet->dstpr = dstpr;
    memcpy(packet->srchw, srchw, MAC_ADDR_LEN);
    memcpy(packet->dsthw, dsthw, MAC_ADDR_LEN);
}
