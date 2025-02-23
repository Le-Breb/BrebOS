#include "DHCP.h"

#include "Endianness.h"
#include "Network.h"
#include "UDP.h"
#include "../core/memory.h"

size_t DHCP::get_response_size(const packet_t* packet)
{
    if (packet->hlen != sizeof(packet_t))
        return -1;
    return sizeof(packet_t);
}

void DHCP::send_discover()
{
    auto buf = (uint8_t*)calloc(1, get_header_size() + UDP::get_headers_size());
    auto buf_beg = buf;
    buf = UDP::write_headers(buf, DHCP_DISC_SRC_PORT, DHCP_DISC_DST_PORT, get_header_size(),
                             *(uint32_t*)&Network::broadcast_ip, (uint8_t*)Network::broadcast_mac);
    write_header(buf, DHCP_DISCOVER, DHCP_FLAG_BROADCAST, 0, 0, 0, 0);

    auto ethernet = (Ethernet::packet_t*)buf_beg;
    auto packet_info = Ethernet::packet_info(ethernet, UDP::get_headers_size() + get_header_size());
    Network::send_packet(&packet_info);
}

void DHCP::write_header(uint8_t* buf, uint8_t op, uint16_t flags, uint32_t ciaddr, uint32_t yiaddr, uint32_t siaddr,
                        uint32_t giaddr)
{
    auto packet = (packet_t*)buf;
    packet->op = op;
    packet->htype = DHCP_HTYPE_ETHERNET;
    packet->hlen = MAC_ADDR_LEN;
    packet->hops = 0;
    packet->xid = Endianness::switch32(0x12345678);
    packet->secs = 0;
    packet->flags = flags;
    packet->ciaddr = ciaddr;
    packet->yiaddr = yiaddr;
    packet->siaddr = siaddr;
    packet->giaddr = giaddr;
    memcpy(packet->chaddr, Network::mac, MAC_ADDR_LEN);
    packet->magic_cookie = Endianness::switch32(DHCP_MAGIC_COOKIE);
}

size_t DHCP::get_header_size()
{
    return sizeof(packet_t);
}
