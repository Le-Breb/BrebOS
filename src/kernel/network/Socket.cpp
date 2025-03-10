#include "Socket.h"

#include "Endianness.h"
#include "Network.h"
#include "../core/fb.h"

list<Socket*> Socket::sockets = {};

void Socket::acknowledge(const TCP::header_t* packet, uint8_t* response_buf)
{
    ack_num = Endianness::switch32(packet->seq_num) + 1; // Set next expected packet
    TCP::write_ack_header(response_buf, this);
}

void Socket::flush_waiting_queue() // Todo: take peer window size into account
{
    for (uint i = 0; i < waiting_queue_size; i++)
    {
        // Get packet
        auto idx = (waiting_queue_start + i) % WAITING_QUEUE_CAPACITY;
        auto packet = waiting_queue[idx];

        // Send packet
        send_data(packet->data, packet->size);

        // Clear queue entry
        waiting_queue[idx] = nullptr;
    }
}

uint8_t* Socket::create_packet_response(uint16_t payload_size, const IPV4::packet_t* ipv4_packet,
                                        const Ethernet::packet_t* ethernet_packet,
                                        Ethernet::packet_info_t& response_info)
{
    return IPV4::create_packet(payload_size, IPV4_PROTOCOL_TCP, ipv4_packet->header.saddr,
                               (uint8_t*)ethernet_packet->header.src, response_info);
}

Socket::Socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port) : peer_port(peer_port)
{
    memcpy(ip, Network::ip, IPV4_ADDR_LEN);
    memcpy(this->peer_ip, peer_ip, IPV4_ADDR_LEN);

    while (port_used(port))
        port = TCP::random_ephemeral_port();

    sockets.add(this);
}

void Socket::set_listener(TCP_listener* listener)
{
    this->listener = listener;
}

Socket::~Socket()
{
    sockets.remove(this);
}

void Socket::initialize()
{
    state = TCP::State::SYN_SENT;
    TCP::send_sync(this);
}

void Socket::handle_packet(const TCP::packet_info_t* packet_info, const IPV4::packet_t* ipv4_packet,
                           const Ethernet::packet_t* ethernet_packet)
{
    auto packet = packet_info->packet;
    auto flags = packet->flags;
    if (flags == (TCP_FLAG_SYN | TCP_FLAG_ACK) && state == TCP::State::SYN_SENT) // SYN response
    {
        seq_num++; // We are about to send a new packet, update seq
        Ethernet::packet_info_t response_info;
        acknowledge(
            packet, create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet, response_info));
        state = TCP::State::ESTABLISHED;

        Network::send_packet(&response_info);

        // Now that connection is established, we can send any packet we've been asked to send so far
        flush_waiting_queue();
    }
    else if (flags == TCP_FLAG_ACK && state == TCP::State::ESTABLISHED) // Peer acknowledges it has received our packet
        return;
    else if (flags == (TCP_FLAG_FIN | TCP_FLAG_ACK) && state == TCP::State::FIN_WAIT_1) // Peer confirms connection end
    {
        // Acknowledge we received the confirmation and definitely close the connection
        Ethernet::packet_info_t response_info;
        acknowledge(
            packet, create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet, response_info));

        Network::send_packet(&response_info);

        state = TCP::State::CLOSED;
    }
    else if (flags == (TCP_FLAG_PSH | TCP_FLAG_ACK) && state == TCP::State::ESTABLISHED) // Data received
    {
        Ethernet::packet_info_t response_info;
        acknowledge(
            packet, create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet, response_info));

        Network::send_packet(&response_info);

        // Trigger data received callback
        if (listener != nullptr)
        {
            auto header_size = packet->header_len * sizeof(uint32_t); // Todo: rewrite that
            auto payload = (uint8_t*)packet + header_size;
            auto payload_size = Endianness::switch16(ipv4_packet->header.len) - header_size;
            listener->on_data_received(payload, payload_size);
        }
    }
    else if (flags == (TCP_FLAG_RST | TCP_FLAG_ACK) && state == TCP::State::SYN_SENT) // Peer port is very likely to be closed
    {
        // Trigger connection error callback
        if (listener)
            listener->on_connection_error();

        printf_error("TCP handshake with %d.%d.%d.%d on port %d failed", peer_ip[0], peer_ip[1],
                     peer_ip[2], peer_ip[3], peer_port);

        state = TCP::State::CLOSED;
    }
    else // Not handled, force close connection
    {
        Ethernet::packet_info_t response_info;
        auto buf = create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet, response_info);
        TCP::write_reset_header(buf, this);

        Network::send_packet(&response_info);
    }
}

Socket* Socket::port_used(uint16_t port)
{
    for (int i = 0; i < sockets.size(); i++)
    {
        auto socket = *sockets.get(i);
        if (socket->port == port)
            return socket;
    }

    return nullptr;
}

void Socket::close_all_connections()
{
    bool connection_open = true;
    while (connection_open)
    {
        connection_open = false;
        for (int i = 0; i < sockets.size(); i++)
        {
            auto socket = *sockets.get(i);
            connection_open = connection_open || (socket->state != TCP::State::CLOSED);
            if (socket->state != TCP::State::ESTABLISHED)
                continue;
            TCP::send_fin_ack(socket);
        }
    }
}

Socket* Socket::reset_response_socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port, uint32_t peer_seq_num)
{
    auto socket = new Socket(peer_ip, peer_port);
    socket->ack_num = peer_seq_num + 1;
    return socket;
}

void Socket::send_data(const void* data, uint16_t size)
{
    // Connection not ready, save enqueue packet and initialize
    if (state == TCP::State::CLOSED)
    {
        if (waiting_queue_size == WAITING_QUEUE_CAPACITY)
        {
            printf_error("Socket waiting queue is full\n");
            return;
        }
        waiting_queue[(waiting_queue_start + waiting_queue_size++) % WAITING_QUEUE_CAPACITY] = new packet_t{data, size};
        initialize();
    }
    else // Send data
    {
        TCP::send_data(data, size, this);
        seq_num += size;
        delete[] (char*)data;
    }
}
