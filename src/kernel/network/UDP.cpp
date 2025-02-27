#include "UDP.h"

#include "DHCP.h"
#include "Endianness.h"
#include "IPV4.h"
#include "Network.h"
#include "../core/fb.h"

void UDP::write_header(uint8_t* buf, uint16_t src_port, uint16_t dst_port, uint16_t payload_size)
{
    auto header = (header_t*)buf;
    size_t size = get_header_size() + payload_size;
    header->src_port = Endianness::switch16(src_port);
    header->dst_port = Endianness::switch16(dst_port);
    header->length = Endianness::switch16(size);
    header->checksum = 0; // UDP checksum is not mandatory over IPV4 (+it includes pseudo-header, it's easier zeroing it)
}

uint8_t* UDP::write_headers(uint8_t* buf, uint16_t src_port, uint16_t dst_port, uint16_t payload_size, uint32_t daddr,
                            uint8_t dst_mac[6])
{
    size_t size = get_header_size() + payload_size;
    buf = IPV4::write_headers(buf, size, IPV4_PROTOCOL_UDP, daddr, dst_mac);
    write_header(buf, src_port, dst_port, payload_size);

    return buf + get_header_size();
}

void UDP::handlePacket(const packet_info_t* packet_info, [[maybe_unused]] uint8_t* response_buffer)
{
    if (DHCP::handle_packet(packet_info->packet))
        return;

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
    return get_header_size() + IPV4::get_headers_size();
}

size_t UDP::get_header_size()
{
    return sizeof(header_t);
}
