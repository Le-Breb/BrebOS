#include "ICMP.h"

#include "Endianness.h"
#include "Network.h"
#include "../core/fb.h"

void ICMP::handle_packet(const packet_info_t* packet_info, const IPV4::packet_t* ipv4_packet,
                         const Ethernet::packet_t* ethernet_packet)
{
    if (!packet_valid(packet_info))
        return;

    auto size = get_response_size(packet_info);
    Ethernet::packet_info_t response_info;
    auto buf = IPV4::create_packet(size, IPV4_PROTOCOL_ICMP, ipv4_packet->header.saddr,
                                   (uint8_t*)ethernet_packet->header.src, response_info);
    write_ping_reply(buf, packet_info);
    Network::send_packet(&response_info);
}

uint16_t ICMP::get_packet_size()
{
    return sizeof(packet_t);
}

uint16_t ICMP::get_response_size(const packet_info_t* request)
{
    return request->size;
}


bool ICMP::packet_valid(const packet_info_t* packet_info)
{
    if (packet_info->size < get_packet_size())
        return false;

    if (!(packet_info->packet->header.type & ICMP_ECHO_REQUEST))
    {
        printf_error("ICMP packet is not an echo request, this is not supported");
        return false;
    }

    if (packet_info->packet->header.code)
    {
        printf_error("ICMP echo request should have a code of 0");
        return false;
    }

    if (Network::checksum(packet_info->packet, packet_info->size))
    {
        printf_error("ICMP echo request checksum failed");
        return false;
    }

    return true;
}

void ICMP::write_ping_reply(uint8_t* response_buf, const packet_info_t* ping_request)
{
    auto header = (header_t*)response_buf;
    header->type = ICMP_ECHO_REPLY;
    header->code = 0;
    memcpy(response_buf + sizeof(header_t), ping_request->packet->payload, ping_request->size - sizeof(header_t));
    header->csum = Network::checksum(response_buf, ping_request->size);
}
