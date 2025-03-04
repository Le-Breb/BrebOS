#include "TCP.h"

#include "ARP.h"
#include "IPV4.h"
#include "Network.h"
#include "../core/memory.h"

uint32_t TCP::sequence_number = 0;
uint32_t TCP::sequence_number2 = 0;
uint16_t TCP::port = 0;
uint16_t TCP::port2 = 1234;
uint8_t TCP::dest_ip[IPV4_ADDR_LEN] = {};
TCP::State TCP::state = State::CLOSED;

// Todo: take into account other side's port instead of always using 1234


uint16_t TCP::random_ephemeral_port()
{
    return 49152 + (Network::generate_random_id32() % (65535 - 49152 + 1));
}

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

void TCP::write_header(uint8_t* buf, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, uint32_t seq_num,
                       uint32_t ack_num, uint8_t flags)
{
    auto header = (header_t*)buf;
    header->src_port = Endianness::switch16(src_port);
    header->dest_port = Endianness::switch16(dest_port);
    header->seq_num = Endianness::switch32(seq_num);
    header->ack_num = Endianness::switch32(ack_num);
    header->header_len = (get_header_size() / sizeof(uint32_t)) << 4;
    header->flags = flags;
    header->window_size = Endianness::switch16(DEFAULT_TCP_WINDOW_SIZE);
    header->checksum = compute_checksum(header, dest_ip, 0);
}

void TCP::write_sync_header(uint8_t* buf, uint32_t dest_ip)
{
    sequence_number = Network::generate_random_id32();
    port = random_ephemeral_port();

    write_header(buf, dest_ip, port, port2, sequence_number, 0, TCP_FLAG_SYN);
}

void TCP::write_ack_header(uint8_t* buf, uint32_t dest_ip, const header_t* acked_packet)
{
    sequence_number = Endianness::switch32(acked_packet->ack_num);
    sequence_number2 = Endianness::switch32(acked_packet->seq_num) + 1;
    write_header(buf, dest_ip, port, port2, sequence_number, sequence_number2, TCP_FLAG_ACK);
}

void TCP::write_reset_header(uint8_t* buf, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port)
{
    write_header(buf, dest_ip, Endianness::switch16(src_port), Endianness::switch16(dest_port), sequence_number,
                 sequence_number2, TCP_FLAG_RST);

    /*printf("Reset connection with %d. Its port is %d and ours was %d\n", dest_ip, Endianness::switch16(dest_port),
           Endianness::switch16(src_port));*/
}

void TCP::write_fin_header(uint8_t* buf, uint32_t dest_ip)
{
    write_header(buf, dest_ip, port, port2, sequence_number, ++sequence_number2, TCP_FLAG_FIN);
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

void TCP::send_fin(const uint8_t dest_ip[IPV4_ADDR_LEN])
{
    auto header_size = get_header_size();
    auto total_size = header_size + IPV4::get_headers_size();
    auto buf = (uint8_t*)calloc(total_size, 1);
    auto buf_beg = buf;
    uint8_t dest_mac[MAC_ADDR_LEN];
    ARP::get_mac(dest_ip, dest_mac);
    buf = IPV4::write_headers(buf, header_size, IPV4_PROTOCOL_TCP, *(uint32_t*)dest_ip, dest_mac);
    write_fin_header(buf, *(uint32_t*)dest_ip);

    auto ethernet = (Ethernet::packet_t*)buf_beg;
    auto packet_info = Ethernet::packet_info(ethernet, total_size);
    Network::send_packet(&packet_info);

    state = State::FIN_WAIT_1;
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
    if (packet_info->size < sizeof(header_t))
        return -1;

    return get_header_size();
}

uint16_t TCP::handle_packet(const IPV4::packet_t* packet, const header_t* tcp_packet, uint8_t* response_buf)
{
    auto flags = tcp_packet->flags;

    if (flags == (TCP_FLAG_SYN | TCP_FLAG_ACK) && state == State::SYN_SENT) // SYN response
    {
        memcpy(dest_ip, &packet->header.saddr, IPV4_ADDR_LEN);
        write_ack_header(response_buf, packet->header.saddr, tcp_packet);
        state = State::ESTABLISHED;
    }
    else if (flags == (TCP_FLAG_FIN | TCP_FLAG_ACK) && state == State::ESTABLISHED) // Other side wants to end connection
    {
        write_ack_header(response_buf, packet->header.saddr, tcp_packet);
        state = State::CLOSED;
    }
    else // Not handled, force close connection
        write_reset_header(response_buf, packet->header.saddr, tcp_packet->dest_port, tcp_packet->src_port);

    return get_header_size();
}

void TCP::close_all_connections()
{
    if (state != State::ESTABLISHED)
        return;
    send_fin(dest_ip);
}

uint16_t TCP::get_headers_size()
{
    return get_header_size() + IPV4::get_headers_size();
}
