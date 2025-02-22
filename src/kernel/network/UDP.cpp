#include "UDP.h"

#include "IPV4.h"
#include "Network.h"
#include "../core/fb.h"

void UDP::write_header(uint8_t* payload_beg, uint16_t src_port, uint16_t dst_port, uint16_t payload_length)
{
    auto header = (header_t*)(payload_beg - sizeof(header_t));
    header->src_port = src_port;
    header->dst_port = dst_port;
    header->length = sizeof(header_t) + payload_length;
    header->checksum = Network::checksum(header, payload_length);
}

void UDP::handlePacket(const packet_info_t* packet_info, [[maybe_unused]] uint8_t* response_buffer)
{
    FB::set_fg(FB_CYAN);
    printf("UDP payload: ");
    for (uint i = 0; i < packet_info->size - sizeof(header_t); ++i)
        printf("%c", packet_info->packet->data[i]);
    FB::set_fg(FB_WHITE);
}

size_t UDP::get_response_size([[maybe_unused]] const packet_info_t* packet_info)
{
    return 0; // Todo: implement that
}

size_t UDP::get_headers_size()
{
    return sizeof(header_t) + IPV4::get_headers_size();
}
