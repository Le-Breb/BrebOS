#include "ICMP.h"

#include "Endianness.h"
#include "Network.h"
#include "../core/fb.h"

void ICMP::handle_packet(const packet_info_t* packet_info, uint8_t* response_buf)
{
    write_ping_reply(packet_info, response_buf);
}

size_t ICMP::get_response_size(const packet_info_t* packet_info)
{
    if (!(packet_info->packet->header.type & ICMP_ECHO_REQUEST))
    {
        printf_error("ICMP packet is not an echo request, this is not supported");
        return -1;
    }

    if (packet_info->packet->header.code)
    {
        printf_error("ICMP echo request should have a code of 0");
        return -1;
    }

    if (Network::checksum(packet_info->packet, packet_info->size))
    {
        printf_error("ICMP echo request checksum failed");
        return -1;
    }

    return packet_info->size; // Response is the same size as the request
}

void ICMP::write_ping_reply(const packet_info_t* ping_request, uint8_t* response_buf)
{
    auto header = (header_t*)response_buf;
    header->type = ICMP_ECHO_REPLY;
    header->code = 0;
    memcpy(response_buf + sizeof(header_t), ping_request->packet->payload, ping_request->size - sizeof(header_t));
    header->csum = Network::checksum(response_buf, ping_request->size);
}
