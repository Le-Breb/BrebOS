#include "Socket.h"

#include <kstdio.h>

#include "DNS.h"
#include "Endianness.h"
#include "Network.h"
#include "../core/fb.h"

list<Socket*> Socket::sockets = {};

void Socket::acknowledge(const TCP::packet_info_t* packet_info, uint8_t* response_buf, bool fin)
{
    auto payload_size = packet_info->size - (packet_info->packet->header_len >> 4) * sizeof(uint32_t);
    ack_num = Endianness::switch32(packet_info->packet->seq_num) + payload_size; // Set next expected packet;
    if (packet_info->packet->flags & (TCP_FLAG_SYN | TCP_FLAG_FIN)) // SYN and FIN consume a sequence number
        ack_num++;
    TCP::write_ack_header(response_buf, this, fin);
}

uint8_t* Socket::create_packet_response(uint16_t payload_size, const IPV4::packet_t* ipv4_packet,
                                        const Ethernet::packet_t* ethernet_packet,
                                        Ethernet::packet_info_t& response_info)
{
    return IPV4::create_packet(payload_size, IPV4_PROTOCOL_TCP, ipv4_packet->header.saddr,
                               (uint8_t*)ethernet_packet->header.src, response_info);
}

void Socket::close(const char* error_message)
{
    state = TCP::State::CLOSED;

    if (!listener)
    {
        if (error_message)
            printf_warn("%s\n", error_message);
        return;
    }

    listener->on_connection_terminated(error_message);
}

const uint8_t* Socket::get_peer_ip() const
{
    return peer_ip;
}

uint16_t Socket::get_peer_port() const
{
    return peer_port;
}

const char* Socket::get_hostname() const
{
    return hostname;
}

void Socket::on_hostname_resolved(const uint8_t peer_ip[4])
{
    memcpy(this->peer_ip, peer_ip, IPV4_ADDR_LEN);
    send_data(start_packet.data, start_packet.size);
}

Socket::Socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port) : hostname(nullptr), peer_port(peer_port)
{
    memcpy(ip, Network::ip, IPV4_ADDR_LEN);
    memcpy(this->peer_ip, peer_ip, IPV4_ADDR_LEN);

    while (port_used(port))
        port = Network::random_ephemeral_port();

    sockets.add(this);
}

Socket::Socket(const char* hostname, uint16_t peer_port) : Socket(strcmp("host", hostname) ? Network::null_ip :
    Network::gateway_ip, peer_port)
{
    if (!strcmp("host", hostname))
        return;
    auto hostname_len = strlen(hostname);
    this->hostname = new char[hostname_len + 1];
    memcpy((char*)this->hostname, hostname, hostname_len + 1);
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

    if (flags & (TCP_FLAG_NS | TCP_FLAG_CWR | TCP_FLAG_URG)) // Not handled, force close connection
    {
        Ethernet::packet_info_t response_info;
        auto buf = create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet, response_info);
        TCP::write_reset_header(buf, this);

        Network::send_packet(&response_info);
        return;
    }

    if (flags & TCP_FLAG_SYN && flags && TCP_FLAG_ACK && state == TCP::State::SYN_SENT) // SYN response
    {
        seq_num++; // We are about to send a new packet, update seq
        Ethernet::packet_info_t response_info;
        acknowledge(
            packet_info, create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet, response_info));
        state = TCP::State::ESTABLISHED;

        Network::send_packet(&response_info);

        // Now that connection is established, send the first packet
        send_data(start_packet.data, start_packet.size);
        return;
    }
    if (flags == TCP_FLAG_ACK && state == TCP::State::CLOSE_WAIT)
    {
        close();
        return;
    }
    if (flags & TCP_FLAG_FIN && flags & TCP_FLAG_ACK && state == TCP::State::FIN_WAIT_1) // Peer confirms connection end
    {
        // Acknowledge we received the confirmation and definitely close the connection
        Ethernet::packet_info_t response_info;
        acknowledge(
            packet_info, create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet, response_info));

        Network::send_packet(&response_info);

        close();
        return;
    }
    if (flags == (TCP_FLAG_RST | TCP_FLAG_ACK) && state == TCP::State::SYN_SENT)
    // Peer port is very likely to be closed
    {
        char error_msg[100]{};
        sprintf(error_msg, "TCP handshake with %d.%d.%d.%d on port %d failed", peer_ip[0], peer_ip[1],
                peer_ip[2], peer_ip[3], peer_port);

        close(error_msg);
        return;
    }
    if (flags & TCP_FLAG_ACK && state == TCP::State::ESTABLISHED)
    {
        auto header_size = (packet->header_len >> 4) * sizeof(uint32_t);
        auto payload = (uint8_t*)packet + header_size;
        auto payload_size = packet_info->size - header_size;

        bool fin = flags & TCP_FLAG_FIN;
        if (payload_size || fin) // Data received
        {
            Ethernet::packet_info_t response_info;
            auto response_buf = create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet,
                                                       response_info);
            acknowledge(packet_info, response_buf, fin);
            if (fin)
                state = TCP::State::CLOSE_WAIT;

            Network::send_packet(&response_info);

            // Trigger data received callback
            if (listener != nullptr)
                listener->on_data_received(payload, payload_size);
        }

        return;
    }

    bool reset = flags & TCP_FLAG_RST;
    if (!reset)
    {
        // Not handled, force close connection
        Ethernet::packet_info_t response_info;
        auto buf = create_packet_response(TCP::get_header_size(), ipv4_packet, ethernet_packet, response_info);
        TCP::write_reset_header(buf, this);

        Network::send_packet(&response_info);
    }

    char error_message[100]{};
    if (reset)
        strcpy(error_message, "Peer closed connection");
    else
        sprintf(error_message, "Unknown TCP packet received. Flags: %x", flags);
    close(error_message);
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
        start_packet.data = data;
        start_packet.size = size;
        // Resolve hostname
        if (!memcmp(Network::null_ip, peer_ip, IPV4_ADDR_LEN))
            DNS::resolve_hostname(hostname, this);
        else initialize(); // Initiate handshake
    }
    else // Send data
    {
        TCP::send_data(data, size, this);
        seq_num += size;
        delete[] (char*)data;
    }
}
