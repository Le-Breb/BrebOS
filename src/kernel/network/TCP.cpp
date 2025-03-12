#include "TCP.h"

#include "ARP.h"
#include "IPV4.h"
#include "Network.h"
#include "../core/memory.h"
#include "Socket.h"
#include "../core/fb.h"

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

void TCP::write_header(uint8_t* buf, const Socket* socket, uint8_t flags, uint16_t data_size)
{
    auto header = (header_t*)buf;
    header->src_port = Endianness::switch16(socket->port);
    header->dest_port = Endianness::switch16(socket->peer_port);
    header->seq_num = Endianness::switch32(socket->seq_num);
    header->ack_num = Endianness::switch32(socket->ack_num);
    header->header_len = (get_header_size() / sizeof(uint32_t)) << 4;
    header->flags = flags;
    header->window_size = Endianness::switch16(DEFAULT_TCP_WINDOW_SIZE);
    header->checksum = compute_checksum(header, *(uint32_t*)socket->peer_ip, data_size);
}

void TCP::write_sync_header(uint8_t* buf, const Socket* socket)
{
    write_header(buf, socket, TCP_FLAG_SYN, 0);
}

void TCP::write_ack_header(uint8_t* buf, const Socket* socket, bool fin)
{
    auto flags = TCP_FLAG_ACK;
    if (fin)
        flags |= TCP_FLAG_FIN;
    write_header(buf, socket, flags, 0);
}

void TCP::write_reset_header(uint8_t* buf, const Socket* socket)
{
    write_header(buf, socket, TCP_FLAG_RST, 0);

    /*printf("Reset connection with %d. Its port is %d and ours was %d\n", dest_ip, Endianness::switch16(dest_port),
           Endianness::switch16(src_port));*/
}

void TCP::write_fin_ack_header(uint8_t* buf, const Socket* socket)
{
    write_header(buf, socket, TCP_FLAG_FIN | TCP_FLAG_ACK, 0);
}

void TCP::write_push_ack_header(uint8_t* buf, const Socket* socket, uint16_t data_size)
{
    write_header(buf, socket, TCP_FLAG_PSH | TCP_FLAG_ACK, data_size);
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

void TCP::send_fin_ack(Socket* socket)
{
    auto header_size = get_header_size();
    Ethernet::packet_info_t response_info;
    uint8_t dest_mac[MAC_ADDR_LEN];
    ARP::get_mac(socket->peer_ip, dest_mac);
    auto buf = IPV4::create_packet(header_size, IPV4_PROTOCOL_TCP, *(uint32_t*)socket->peer_ip, dest_mac,
                                   response_info);
    write_fin_ack_header(buf, socket);

    socket->seq_num++;
    socket->state = State::FIN_WAIT_1;
    Network::send_packet(&response_info);
}

void TCP::send_sync(const Socket* socket)
{
    auto header_size = get_header_size();
    Ethernet::packet_info_t response_info;
    uint8_t dest_mac[MAC_ADDR_LEN];
    ARP::get_mac(socket->peer_ip, dest_mac);
    auto buf = IPV4::create_packet(header_size, IPV4_PROTOCOL_TCP, *(uint32_t*)socket->peer_ip, dest_mac,
                                   response_info);
    write_sync_header(buf, socket);

    Network::send_packet(&response_info);
}

bool TCP::packet_valid(const packet_info_t* packet_info)
{
    return packet_info->size >= get_header_size();
}

uint16_t TCP::handle_packet(const packet_info_t* packet_info, const IPV4::packet_t* ipv4_packet,
                            const Ethernet::packet_t* ethernet_packet)
{
    auto packet = packet_info->packet;
    Socket* socket = nullptr;
    if ((socket = Socket::port_used(Endianness::switch16(packet->dest_port))))
        socket->handle_packet(packet_info, ipv4_packet, ethernet_packet);
    else // Force close connection
    {
        // Create a socket specifically to send the RST, then delete it
        auto response_socket = Socket::reset_response_socket((uint8_t*)&ipv4_packet->header.saddr,
                                                             Endianness::switch16(packet->src_port),
                                                             Endianness::switch32(packet->seq_num));
        auto packet_size = get_header_size();
        Ethernet::packet_info_t response_info;
        auto buf = IPV4::create_packet(packet_size, IPV4_PROTOCOL_TCP, ipv4_packet->header.saddr,
                                       (uint8_t*)ethernet_packet->header.src, response_info);
        write_reset_header(buf, response_socket);
        delete response_socket;
        Network::send_packet(&response_info);
    }

    return get_header_size();
}

void TCP::send_data(const void* data, uint16_t data_size, const Socket* socket)
{
    auto header_size = get_header_size();
    auto tcp_packet_size = header_size + data_size;
    Ethernet::packet_info_t response_info;
    uint8_t dest_mac[MAC_ADDR_LEN];
    ARP::get_mac(socket->peer_ip, dest_mac);
    auto buf = IPV4::create_packet(tcp_packet_size, IPV4_PROTOCOL_TCP, *(uint32_t*)socket->peer_ip, dest_mac,
                                   response_info);
    auto payload = buf + header_size;
    memcpy(payload, data, data_size); // Todo: Extend no-copy scheme to higher level protocols
    write_push_ack_header(buf, socket, data_size);

    Network::send_packet(&response_info);
}
