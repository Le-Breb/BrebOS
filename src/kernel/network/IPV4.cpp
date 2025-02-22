#include "IPV4.h"

#include "Endianness.h"
#include "ICMP.h"
#include "Network.h"
#include "UDP.h"
#include "../core/fb.h"

void IPV4::handlePacket(const packet_t* packet, uint8_t* response_buffer)
{
    size_t header_len = packet->header.get_ihl() * sizeof(uint32_t);
    auto payload_size = Endianness::switch_endian16(packet->header.len) - header_len;
    switch (packet->header.proto)
    {
        case IPV4_PROTOCOL_ICMP:
        {
            auto icmp_packet = (ICMP::packet_t*)((uint8_t*)packet + sizeof(header_t));
            auto icmp_packet_info = ICMP::packet_info_t(icmp_packet, payload_size);
            ICMP::handlePacket(&icmp_packet_info, response_buffer + header_len);
            break;
        }
        case IPV4_PROTOCOL_UDP:
        {
            auto udp_packet = (UDP::packet_t*)((uint8_t*)packet + sizeof(header_t));
            auto udp_packet_info = UDP::packet_info_t(udp_packet, payload_size);
            UDP::handlePacket(&udp_packet_info, response_buffer + header_len);
            break;
        }
        default:
            break;
    }

    if (response_buffer == nullptr)
        return;

    write_response(response_buffer, &packet->header, payload_size);
}

size_t IPV4::get_headers_size()
{
    return sizeof(header_t) + Ethernet::get_headers_size();
}

void IPV4::write_response(uint8_t* response_buf, const header_t* request_header, size_t payload_size)
{
    size_t header_len = request_header->get_ihl() * sizeof(uint32_t);
    size_t len = header_len + payload_size;
    write_packet(response_buf, request_header->get_version(), request_header->get_ihl(), request_header->tos,
                 len, Endianness::switch_endian16(request_header->id), request_header->get_flags(), PV4_DEFAULT_TTL,
                 IPV4_PROTOCOL_ICMP, *(uint32_t*)Network::ip, request_header->saddr);
}

void IPV4::write_packet(uint8_t* buf, uint8_t version, uint8_t ihl, uint8_t tos, uint16_t len, uint16_t id,
                        uint8_t flags, uint8_t ttl, uint8_t proto, uint32_t saddr, uint32_t daddr)
{
    auto reply = (header_t*)buf;
    reply->set_version(version);
    reply->set_ihl(ihl);
    reply->tos = tos;
    reply->len = Endianness::switch_endian16(len);
    reply->id = Endianness::switch_endian16(id);
    reply->set_flags(flags);
    reply->set_frag_offset_to_zero();
    reply->ttl = ttl;
    reply->proto = proto;
    reply->saddr = saddr;
    reply->daddr = daddr;
    reply->csum = Network::checksum(buf, len);
}

size_t IPV4::get_response_size(const Ethernet::packet_info_t* packet_info)
{
    auto header = &((packet_t*)packet_info->packet->payload)->header;
    if (header->get_version() != 4)
        return -1; // Not IPV4 - wtf, when can this happen ?
    // This packet is not destined to us
    if (!(header->daddr == *(uint32_t*)Network::ip || header->daddr == *(uint32_t*)Network::broadcast_ip))
        return -1;
    if (header->get_frag_offset() != 0 || header->get_flags() & IPV4_FLAG_MORE_FRAGMENTS)
    {
        printf_error("Fragmented IP packet received, not handled yet");
        return -1;
    }
    if (Network::checksum(header, sizeof(*header)))
    {
        printf_error("IPV4 checksum error");
        return -1;
    }

    size_t size = header->get_ihl() * sizeof(uint32_t);
    size_t inner_size = 0;
    switch (header->proto)
    {
        case IPV4_PROTOCOL_ICMP:
        {
            auto icmp_packet = (ICMP::packet_t*)((uint8_t*)header + size);
            ICMP::packet_info_t icmp(icmp_packet, Endianness::switch_endian16(header->len) - size);
            inner_size = ICMP::get_response_size(&icmp);
            break;
        }
        case IPV4_PROTOCOL_UDP:
        {
            auto udp_packet = (UDP::packet_t*)((uint8_t*)header + size);
            UDP::packet_info_t udp(udp_packet, Endianness::switch_endian16(header->len) - size);
            inner_size = UDP::get_response_size(&udp);
            break;
        }
        default:
            printf_error("IP Protocol not ICMP or UDP (is 0x%2x), not supported yet", header->proto);
        return -1;
    }

    if (inner_size == 0 || inner_size == (size_t)-1)
        return inner_size;
    return size + inner_size;
}
