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

uint8_t* Ethernet::create_packet(uint8_t dest[6], uint16_t type, uint16_t payload_size, packet_info_t& packet_info)
{
    auto packet_size = get_header_size() + payload_size;
    auto buf = (uint8_t*)calloc(packet_size, sizeof(uint8_t));
    packet_info.size = packet_size;
    packet_info.packet = (packet_t*)buf;

    return write_header(buf, dest, type);
}

void Ethernet::handle_packet(const packet_info_t* packet_info)
{
    if (packet_info->size < get_header_size())
        return;

    auto type = Endianness::switch16(packet_info->packet->header.type);

    switch (type)
    {
        case ETHERTYPE_ARP:
        {
            ARP::handle_packet((ARP::packet_t*)packet_info->packet->payload, packet_info->packet);
            break;
        }
        case ETHERTYPE_IPV4:
        {
            IPV4::handle_packet((IPV4::packet_t*)packet_info->packet->payload, packet_info->packet);
            break;
        }
        default:
            break; // Not supported
    }
}

size_t Ethernet::get_header_size()
{
    return sizeof(header_t);
}
