#include "Socket.h"

#include "Endianness.h"
#include "Network.h"
#include "../core/fb.h"

list<Socket*> Socket::sockets = {};
uint16_t Socket::socket_head = 0;

void Socket::acknowledge(const TCP::header_t* packet, uint8_t* response_buf)
{
    ack_num = Endianness::switch32(packet->seq_num) + 1;
    TCP::write_ack_header(response_buf, this);
}

Socket::Socket(uint8_t peer_ip[IPV4_ADDR_LEN], uint16_t peer_port) : peer_port(peer_port)
{
    memcpy(ip, Network::ip, IPV4_ADDR_LEN);
    memcpy(this->peer_ip, peer_ip, IPV4_ADDR_LEN);

    while (port_used(port))
        port = TCP::random_ephemeral_port();

    sockets.add(this);
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

void Socket::handle_packet(const TCP::header_t* packet, uint8_t* response_buf)
{
    auto flags = packet->flags;
    if (flags == (TCP_FLAG_SYN | TCP_FLAG_ACK) && state == TCP::State::SYN_SENT) // SYN response
    {
        seq_num++;
        acknowledge(packet, response_buf);
        state = TCP::State::ESTABLISHED;
        printf("State is established\n");
    }
    else if (flags == (TCP_FLAG_FIN | TCP_FLAG_ACK) && state == TCP::State::ESTABLISHED)
        // Other side wants to end connection
    {
        acknowledge(packet, response_buf);
        state = TCP::State::CLOSED;
    }
    else // Not handled, force close connection
        TCP::write_reset_header(response_buf, this);
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
    for (int i = 0; i < sockets.size(); i++)
    {
        auto socket= *sockets.get(i);
        if (socket->state != TCP::State::ESTABLISHED)
            continue;
        socket->ack_num++;
        TCP::send_fin(socket);
        socket->state = TCP::State::FIN_WAIT_1;
    }
}

Socket* Socket::reset_response_socket(uint8_t peer_ip[4], uint16_t peer_port, uint32_t peer_seq_num)
{
    auto socket = new Socket(peer_ip, peer_port);
    socket->ack_num = peer_seq_num + 1;
    return socket;
}


