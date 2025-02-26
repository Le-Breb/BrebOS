#include "Ethernet.h"

#include "ARP.h"
#include "Endianness.h"
#include "IPV4.h"
#include "Network.h"
#include "../core/memory.h"

uint8_t* Ethernet::write_header(uint8_t* buf, uint8_t dest[MAC_ADDR_LEN], uint16_t type)
{
    auto header = (header_t*)buf;
    memcpy(header->dest, dest, MAC_ADDR_LEN);
    memcpy(header->src, Network::mac, MAC_ADDR_LEN);
    header->type = Endianness::switch16(type);

    return (uint8_t*)(header + 1);;
}

size_t Ethernet::get_response_size(const packet_info* packet_info)
{
    auto type = Endianness::switch16(packet_info->packet->header.type);
    size_t size = sizeof(header_t);
    size_t inner_size = -1;
    switch (type)
    {
        case ETHERTYPE_ARP:
            inner_size = ARP::get_response_size((ARP::packet_t*)packet_info->packet->payload);
            break;
        case ETHERTYPE_IPV4:
            inner_size = IPV4::get_response_size((IPV4::packet_t*)packet_info->packet->payload);
            break;
        default:
            break;
    }

    if (inner_size == 0 || inner_size == (size_t)-1)
        return inner_size;
    return size + inner_size;
}

void Ethernet::handle_packet(const packet_info_t* packet_info)
{
    auto type = Endianness::switch16(packet_info->packet->header.type);
    size_t response_size = get_response_size(packet_info);

    if (response_size == (size_t)-1) // Error or no need to process this packet
        return;

    bool response_needed = response_size != 0;
    uint8_t* response_buf = response_needed ?  (uint8_t*)calloc(1, response_size) : nullptr;
    uint8_t* response_beg = response_buf;

    if (response_needed)
        response_buf = write_header(response_buf, packet_info->packet->header.src, type);

    switch (type)
    {
        case ETHERTYPE_ARP:
        {
            ARP::handlePacket((ARP::packet_t*)packet_info->packet->payload, response_buf);
            break;
        }
        case ETHERTYPE_IPV4:
        {
            IPV4::handlePacket((IPV4::packet_t*)packet_info->packet->payload, response_buf);
            break;
        }
        default:
            break; // Not supported
    }

    if (response_needed)
    {
        auto resp_packet_info = packet_info_t((packet_t*)response_beg, response_size);
        Network::send_packet(&resp_packet_info);
    }
}

size_t Ethernet::get_headers_size()
{
    return sizeof(header_t);
}
