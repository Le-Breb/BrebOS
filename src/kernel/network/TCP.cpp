#include "TCP.h"

#include "ARP.h"
#include "IPV4.h"
#include "Network.h"
#include "../core/memory.h"

uint32_t TCP::sequence_number = 0;
uint32_t TCP::sequence_number2 = 0;
TCP::State TCP::state = State::CLOSED;

// Todo: take into account other side's port instead of always using 1234


void TCP::fill_pseudo_header(pseudo_header_t& pseudo_header, const header_t* header, uint32_t dest_ip,
                             uint16_t payload_size)
{
    pseudo_header.src_ip = *(uint32_t*)Network::ip;
    pseudo_header.dest_ip = dest_ip;
    //pseudo_header.reserved = 0;
    pseudo_header.protocol = IPV4_PROTOCOL_TCP;
    pseudo_header.tcp_len = Endianness::switch16((header->header_len >> 4) * sizeof(uint32_t) + payload_size);
}

size_t TCP::get_header_size()
{
    return (sizeof(header_t) + (sizeof(uint32_t) - 1)) & ~(sizeof(uint32_t) - 1); // Align to 4 bytes
}

void TCP::write_sync_header(uint8_t* buf, uint32_t dest_ip)
{
    sequence_number = Network::generate_random_id32();

    auto header = (header_t*)buf;
    header->src_port = Endianness::switch16(TCP_PORT);
    header->dest_port = Endianness::switch16(TCP_PORT);
    header->seq_num = Endianness::switch32(sequence_number);
    header->ack_num = 0;
    header->header_len = (get_header_size() / sizeof(uint32_t)) << 4;
    header->flags = TCP_FLAG_SYN;
    header->window_size = Endianness::switch16(DEFAULT_TCP_WINDOW_SIZE);
    header->checksum = compute_checksum(header, dest_ip, 0);
}

void TCP::write_syn_ack_header(uint8_t* buf, uint32_t dest_ip)
{
    auto header = (header_t*)buf;
    header->src_port = Endianness::switch16(TCP_PORT);
    header->dest_port = Endianness::switch16(TCP_PORT);
    header->seq_num = Endianness::switch32(++sequence_number);
    header->ack_num = Endianness::switch32(++sequence_number2);
    header->header_len = (get_header_size() / sizeof(uint32_t)) << 4;
    header->flags = TCP_FLAG_ACK;
    header->window_size = Endianness::switch16(DEFAULT_TCP_WINDOW_SIZE);
    header->checksum = compute_checksum(header, dest_ip, 0);
}


uint16_t TCP::compute_checksum(const header_t* header, uint32_t dest_ip, uint16_t payload_size)
{
    // Pseudo header checksum
    pseudo_header_t pseudo_header{};
    fill_pseudo_header(pseudo_header, header, dest_ip, payload_size);
    auto c1 = Network::checksum(&pseudo_header, sizeof(pseudo_header_t));

    // Header and payload checksum
    auto total_packet_size = Endianness::switch16(pseudo_header.tcp_len);
    auto c2 = Network::checksum(header, total_packet_size);

    // Combine the two checksums
    return Network::checksum_add(c1, c2);
}

void TCP::send_sync(const uint8_t dest_ip[IPV4_ADDR_LEN])
{
    auto header_size = get_header_size();
    auto total_size = header_size + IPV4::get_headers_size();
    auto buf = (uint8_t*)calloc(total_size, 1);
    auto buf_beg = buf;
    uint8_t dest_mac[MAC_ADDR_LEN];
    ARP::get_mac(dest_ip, dest_mac);
    buf = IPV4::write_headers(buf, header_size, IPV4_PROTOCOL_TCP, *(uint32_t*)dest_ip, dest_mac);
    write_sync_header(buf, *(uint32_t*)dest_ip);

    auto ethernet = (Ethernet::packet_t*)buf_beg;
    auto packet_info = Ethernet::packet_info(ethernet, total_size);
    Network::send_packet(&packet_info);

    state = State::SYN_SENT;
}

size_t TCP::get_response_size(const packet_info_t* packet_info)
{
    if (state == State::CLOSED)
        return -1;
    if (packet_info->size < sizeof(header_t))
        return -1;

    auto packet = packet_info->packet;
    if (packet->dest_port != Endianness::switch16(TCP_PORT))
        return -1;
    if (state == State::SYN_SENT && packet->flags == (TCP_FLAG_SYN | TCP_FLAG_ACK) && Endianness::switch32(packet->ack_num) == sequence_number + 1)
    {
        state = State::ESTABLISHED;
        sequence_number2 = Endianness::switch32(packet->seq_num);
        return get_header_size();
    }

    return -1;
}

uint16_t TCP::handle_packet(const IPV4::packet_t* packet, uint8_t* response_buf)
{
    auto tcp_packet = (header_t*)((uint8_t*)packet + sizeof(IPV4::header_t));
    auto flags = tcp_packet->flags;
    if (flags == (TCP_FLAG_SYN | TCP_FLAG_ACK))
    {
        write_syn_ack_header(response_buf, packet->header.saddr);
        return get_header_size();
    }

    return 0; // Should not happen
}