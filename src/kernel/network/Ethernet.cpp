#include "Ethernet.h"

#include "ARP.h"
#include "Endianness.h"
#include "IPV4.h"
#include "Network.h"
#include "../core/memory.h"

void Ethernet::write_header(header_t* ptr, uint8_t dest[ETHERNET_MAC_LEN], uint8_t src[ETHERNET_MAC_LEN], uint16_t type)
{
    memcpy(ptr->dest, dest, ETHERNET_MAC_LEN);
    memcpy(ptr->src, src, ETHERNET_MAC_LEN);
    ptr->type = Endianness::switch_endian16(type);
}

size_t Ethernet::get_response_size(packet_info* packet_info)
{
    auto type = Endianness::switch_endian16(packet_info->packet->header.type);
    size_t size = sizeof(header_t);
    size_t inner_size = -1;
    switch (type)
    {
        case ETHERTYPE_ARP:
            inner_size = ARP::get_response_size((ARP::packet*)packet_info->packet->payload);
            break;
        case ETHERTYPE_IPV4:
            inner_size = IPV4::get_response_size((IPV4::header_t*)packet_info->packet->payload);
            break;
        default:
            break;
    }

    if (inner_size == 0 || inner_size == (size_t)-1)
        return inner_size;
    return size + inner_size;
}

void Ethernet::handle_packet(packet_info* packet_info)
{
    auto type = Endianness::switch_endian16(packet_info->packet->header.type);
    size_t response_size = get_response_size(packet_info);

    if (response_size == (size_t)-1) // Error or no need to process this packet
        return;

    uint8_t* response_buf = nullptr;
    if (response_size > 0)
    {
        response_buf = (uint8_t*)calloc(1, response_size);
        write_header((header_t*)response_buf, (uint8_t*)Network::broadcast_mac, Network::mac, type);
        response_buf += sizeof(header_t);
    }
    switch (type)
    {
        case ETHERTYPE_ARP:
        {
            auto arp = (ARP::packet*)(packet_info->packet->payload);
            ARP::handlePacket(arp, response_buf);

            break;
        }
        case ETHERTYPE_IPV4:
        {
            auto ip = (IPV4::header_t*)packet_info->packet->payload;
            IPV4::handlePacket(ip, response_buf);
            break;
        }
        default:
            break; // Not supported
    }

    if (response_size)
    {
        response_buf -= sizeof(header_t);
        auto response_packet_info = new packet_info_t((packet_t*)response_buf, response_size);
        Network::sendPacket(response_packet_info);
    }
}
