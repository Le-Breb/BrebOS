#include "UDP.h"

#include "DHCP.h"
#include "DNS.h"
#include "Endianness.h"
#include "IPV4.h"
#include "TP.h"
#include "../core/fb.h"

uint16_t UDP::write_header(uint8_t* buf, uint16_t src_port, uint16_t dst_port, uint16_t payload_size)
{
    auto header = (header_t*)buf;
    size_t size = get_header_size() + payload_size;
    header->src_port = Endianness::switch16(src_port);
    header->dst_port = Endianness::switch16(dst_port);
    header->length = Endianness::switch16(size);
    header->checksum = 0; // UDP checksum is not mandatory over IPV4 (+it includes pseudo-header, it's easier zeroing it)

    return get_header_size();
}


uint8_t* UDP::create_packet(uint16_t src_port, uint16_t dst_port, uint16_t payload_size, uint32_t daddr,
    uint8_t dst_mac[6], Ethernet::packet_info_t& ethernet_packet_info)
{
    size_t size = get_header_size() + payload_size;
    auto buf = IPV4::create_packet(size, IPV4_PROTOCOL_UDP, daddr, dst_mac, ethernet_packet_info);

    return buf + write_header(buf, src_port, dst_port, payload_size);
}

void UDP::handle_packet(const packet_info_t* packet_info, [[maybe_unused]] const IPV4::packet_t* ipv4_packet, [[maybe_unused]] const Ethernet::packet_t* ethernet_packet)
{
    if (!packet_valid(packet_info))
        return;

    // Let higher level protocols try to handle the payload
    if (DHCP::handle_packet(packet_info->packet))
        return;
    if (DNS::handle_packet(packet_info->packet))
        return;
    if (TP::handle_packet(packet_info->packet))
        return;

    // We received something which is not one of the protocols checked above, then we simply display payload content
    FB::set_fg(FB_CYAN);
    printf("UDP payload: ");
    for (uint i = 0; i < packet_info->size - sizeof(header_t); ++i)
        printf("%c", packet_info->packet->payload[i]);
    FB::set_fg(FB_WHITE);
}

bool UDP::packet_valid(const packet_info_t* packet_info)
{
    return packet_info->size >= get_header_size(); // Todo: implement better checking, checking if port is open etc...
}

size_t UDP::get_header_size()
{
    return sizeof(header_t);
}
