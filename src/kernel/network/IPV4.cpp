#include "IPV4.h"

#include "Endianness.h"
#include "ICMP.h"
#include "Network.h"
#include "../core/fb.h"

void IPV4::handlePacket(header_t* header, uint8_t* response_buffer)
{
    // Grab ICMP packet
    auto size = header->get_ihl() * sizeof(uint32_t);
    auto icmp_packet = (ICMP::packet_t*)((uint8_t*)header + size);
    ICMP::packet_info_t icmp(icmp_packet, Endianness::switch_endian16(header->len) - size);

    ICMP::handlePacket(&icmp, response_buffer + sizeof(header_t));

    if (!response_buffer) // response buffer == null => no response needed
        return;

    write_response(header, response_buffer);
}

void IPV4::write_response(header_t* request_header, uint8_t* response_buffer)
{
    auto* reply = (header_t*)response_buffer;
    memcpy(reply, request_header, 1 + 1 + 2 + 2 + 2 + 1 + 1); // Copy fields up to proto (included)
    reply->saddr = request_header->daddr;
    reply->daddr = request_header->saddr;
    reply->csum = Network::checksum(reply, sizeof(*reply));
}

size_t IPV4::get_response_size(const header_t* header)
{
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
    if (header->proto != IPV4_PROTOCOL_ICMP)
    {
        printf_error("IP Protocol not ICMP (is 0x%2x), not supported yet", header->proto);
        return -1;
    }
    if (Network::checksum(header, sizeof(*header)))
    {
        printf_error("IPV4 checksum error");
        return -1;
    }

    size_t size = header->get_ihl() * sizeof(uint32_t);
    auto icmp_packet = (ICMP::packet_t*)((uint8_t*)header + size);
    ICMP::packet_info_t icmp(icmp_packet, Endianness::switch_endian16(header->len) - size);
    size_t inner_size = ICMP::get_response_size(&icmp);

    if (inner_size == 0 || inner_size == (size_t)-1)
        return inner_size;
    return size + inner_size;
}
