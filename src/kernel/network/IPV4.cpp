#include "IPV4.h"

#include "Endianness.h"
#include "ICMP.h"
#include "Network.h"
#include "UDP.h"
#include "../core/fb.h"

void IPV4::handlePacket(const packet_t* packet, uint8_t* response_buffer)
{
    size_t header_len = packet->header.get_ihl() * sizeof(uint32_t);
    auto payload_size = Endianness::switch16(packet->header.len) - header_len;
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
    return get_header_size() + Ethernet::get_headers_size();
}

size_t IPV4::get_header_size()
{
    return IPV4_DEFAULT_IHL * sizeof(uint32_t);
}

void IPV4::write_response(uint8_t* response_buf, const header_t* request_header, size_t payload_size)
{
    size_t header_len = request_header->get_ihl() * sizeof(uint32_t);
    size_t len = header_len + payload_size;
    write_packet(response_buf, request_header->tos, len, Endianness::switch16(request_header->id),
                 request_header->get_flags(), IPV4_DEFAULT_TTL, IPV4_PROTOCOL_ICMP,
                 request_header->saddr);
}

void IPV4::write_packet(uint8_t* buf, uint8_t tos, uint16_t size, uint16_t id,
                        uint8_t flags, uint8_t ttl, uint8_t proto, uint32_t daddr)
{
    auto reply = (header_t*)buf;
    reply->set_version(4);
    reply->set_ihl(IPV4_DEFAULT_IHL);
    reply->tos = tos;
    reply->len = Endianness::switch16(size);
    reply->id = Endianness::switch16(id);
    reply->set_flags(flags);
    reply->set_frag_offset_to_zero();
    reply->ttl = ttl;
    reply->proto = proto;
    reply->saddr = *(uint32_t*)Network::ip;
    reply->daddr = daddr;
    reply->csum = Network::checksum(buf, size);
}

uint8_t* IPV4::write_headers(uint8_t* buf, uint16_t payload_size, uint8_t proto, uint32_t daddr, uint8_t dst_mac[6])
{
    buf = Ethernet::write_header(buf, dst_mac, ETHERTYPE_IPV4);
    size_t header_size = get_header_size();
    size_t size = header_size + payload_size;
    write_packet(buf, 0, size, 0, 0, IPV4_DEFAULT_TTL, proto, daddr);

    return buf + header_size;
}

bool IPV4::address_is_in_subnet(const uint8_t address[IPV4_ADDR_LEN])
{
    uint8_t masked_address[IPV4_ADDR_LEN];
    uint8_t masked_ip[IPV4_ADDR_LEN];
    for (size_t i = 0; i < IPV4_ADDR_LEN; i++)
    {
        masked_address[i] = address[i] & Network::subnet_mast[i];
        masked_ip[i] = Network::ip[i] & Network::subnet_mast[i];
    }

    return memcmp(masked_address, masked_ip, IPV4_ADDR_LEN) == 0;
}

size_t IPV4::get_response_size(const packet_t* packet)
{
    auto header = &packet->header;
    if (header->get_version() != 4)
        return -1; // Not IPV4 - wtf, when can this happen ?
    if (header->get_ihl() != IPV4_DEFAULT_IHL)
        return -1; // Additional header data, not supported yet
    // This packet is not destined to us
    if (!(header->daddr == *(uint32_t*)Network::ip || header->daddr == *(uint32_t*)Network::broadcast_ip))
        return -1;
    if (header->get_frag_offset() != 0 || header->get_flags() & IPV4_FLAG_MORE_FRAGMENTS)
    {
        printf_error("Fragmented IP packet received, not handled yet");
        return -1;
    }
    if (Network::checksum(header, get_header_size()))
    {
        printf_error("IPV4 checksum error");
        return -1;
    }

    size_t size = get_header_size();
    size_t inner_size = 0;
    switch (header->proto)
    {
        case IPV4_PROTOCOL_ICMP:
        {
            auto icmp_packet = (ICMP::packet_t*)((uint8_t*)header + size);
            ICMP::packet_info_t icmp(icmp_packet, Endianness::switch16(header->len) - size);
            inner_size = ICMP::get_response_size(&icmp);
            break;
        }
        case IPV4_PROTOCOL_UDP:
        {
            auto udp_packet = (UDP::packet_t*)((uint8_t*)header + size);
            UDP::packet_info_t udp(udp_packet, Endianness::switch16(header->len) - size);
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
